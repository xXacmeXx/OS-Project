#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h> // برای کار با thread ها
#include <semaphore.h>
#include <sys/wait.h>
#include <dirent.h>
#include <fcntl.h>  // برای تنظیمات Pipe
typedef struct {
    char product_name[256]; // نام محصول
    double price;           // قیمت محصول
    double score;           // امتیاز محصول
    int entity;             // تعداد موجودی
    char last_modified[30]; // تاریخ آخرین تغییر
} ShoppingItem;

typedef struct {
    ShoppingItem* items; // آرایه‌ای از محصولات
    int item_count;      // تعداد محصولات
} ShoppingList;

// ساختار برای اطلاعات کاربر
typedef struct {
    char name[256];
    ShoppingItem* shopping_list; // اشاره‌گر برای حافظه پویا
    int item_count;              // تعداد محصولات
    double budget;
} UserInfo;


typedef struct {
    char filepath[512];     // مسیر فایل
    int pipe_fd;            // لوله ارتباطی با والد
    char** shopping_list;   // لیست خرید کاربر
    int shopping_list_count;// تعداد محصولات در لیست خرید
} ThreadArgs;


sem_t sem_orders_done;
sem_t sem_final_done;

// لیست منتخب توسط Thread Orders
ShoppingList selected_list;

// تابع Thread Orders
void* orders_thread(void* arg) {
    printf("PID %d create thread for Orders TID: %lu\n", getpid(), pthread_self());

    // شبیه‌سازی دریافت سه لیست خرید از فرایند store
    printf("Orders Thread: Waiting for 3 shopping lists...\n");
    //sleep(2); // شبیه‌سازی تأخیر

    // انتخاب لیست بر اساس معیارها (اینجا فقط یک لیست ساده پر می‌شود)
    printf("Orders Thread: Evaluating lists and selecting the best one...\n");
    selected_list.item_count = 2;
    selected_list.items = malloc(2 * sizeof(ShoppingItem));

    strcpy(selected_list.items[0].product_name, "Apple");
    selected_list.items[0].entity = 3;

    strcpy(selected_list.items[1].product_name, "Banana");
    selected_list.items[1].entity = 2;

    printf("Orders Thread: Best list selected, notifying Final Thread.\n");

    // آزاد کردن semaphore برای شروع Thread Final
    sem_post(&sem_orders_done);

    pthread_exit(NULL);
}

// تابع Thread Final
void* final_thread(void* arg) {
    printf("PID %d create thread for Final TID: %lu\n", getpid(), pthread_self());

    // منتظر شدن برای Thread Orders
    sem_wait(&sem_orders_done);
    printf("Final Thread: Received the selected shopping list. Processing...\n");

    // شبیه‌سازی پردازش لیست منتخب
    //sleep(1); // تأخیر برای پردازش
    printf("Final Thread: Processing completed. Notifying Scores Thread.\n");

    // آزاد کردن semaphore برای شروع Thread Scores
    sem_post(&sem_final_done);

    pthread_exit(NULL);
}

// تابع Thread Scores
void* scores_thread(void* arg) {
    printf("PID %d create thread for Scores TID: %lu\n", getpid(), pthread_self());

    // منتظر شدن برای Thread Final
    sem_wait(&sem_final_done);
    printf("Scores Thread: Requesting scores for each product...\n");

    // درخواست امتیاز از کاربر برای هر محصول
    for (int i = 0; i < selected_list.item_count; i++) {
        int score;
        printf("Enter score for %s (Quantity: %d): ", 
                selected_list.items[i].product_name, 
                selected_list.items[i].entity);
        scanf("%d", &score);
        printf("Score for %s: %d\n", selected_list.items[i].product_name, score);
    }

    free(selected_list.items); // آزادسازی حافظه پویا
    pthread_exit(NULL);
}


void* process_product_file(void* arg) {
    ThreadArgs* args = (ThreadArgs*)arg;

    // باز کردن فایل
    FILE* file = fopen(args->filepath, "r");
    if (!file) {
        perror("Failed to open product file");
        pthread_exit(NULL);
    }

    // خواندن اطلاعات محصول از فایل
    ShoppingItem product = {0};
    fscanf(file, "Name: %[^\n]\n", product.product_name);
    fscanf(file, "Price: %lf\n", &product.price);
    fscanf(file, "Score: %lf\n", &product.score);
    fscanf(file, "Entity: %d\n", &product.entity);
    fscanf(file, "Last Modified: %[^\n]\n", product.last_modified);
    fclose(file);

    // بررسی اینکه آیا محصول در لیست خرید کاربر هست یا نه
    for (int i = 0; i < args->shopping_list_count; i++) {
        if (strcmp(product.product_name, args->shopping_list[i]) == 0) {
            // ارسال محصول به والد از طریق Pipe
            write(args->pipe_fd, &product, sizeof(ShoppingItem));
            printf("Thread for file '%s': Found product '%s', sending to parent.\n",
                   args->filepath, product.product_name);
        }
    }

    pthread_exit(NULL);
}



void handle_category(const char* category_name, const char* category_path, char** shopping_list, int shopping_list_count, int pipe_fd) {
    DIR* dir = opendir(category_path);
    if (!dir) {
        perror("Failed to open category directory");
        exit(1);
    }

    struct dirent* entry;
    pthread_t threads[256]; // حداکثر 256 فایل
    ThreadArgs thread_args[256];
    int thread_count = 0;

    printf("Processing category: %s\n", category_name);

    // خواندن فایل‌های txt در دسته‌بندی
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".txt")) { // بررسی فایل‌های txt
            // ساخت مسیر کامل فایل
            snprintf(thread_args[thread_count].filepath, sizeof(thread_args[thread_count].filepath), "%s/%s", category_path, entry->d_name);

            // تنظیم پارامترهای thread
            thread_args[thread_count].pipe_fd = pipe_fd;
            thread_args[thread_count].shopping_list = shopping_list;
            thread_args[thread_count].shopping_list_count = shopping_list_count;

            // ایجاد thread برای پردازش فایل
            pthread_create(&threads[thread_count], NULL, process_product_file, &thread_args[thread_count]);
            thread_count++;
        }
    }

    // منتظر ماندن برای اتمام همه threadها
    for (int i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
    }

    closedir(dir);
    printf("Finished processing category: %s\n", category_name);
}



// نام دسته‌بندی‌ها (category)
//const char* categories[] = {"Apparel", "Beauty", "Digital", "Food", "Home", "Market", "Sports", "Toys"};

// تابع برای فرایند store
void handle_store(const char* store_name, const char* store_path, char** shopping_list, int shopping_list_count) {
    pid_t category_pids[8]; // شناسه‌های فرایندهای فرزند برای دسته‌بندی‌ها
    const char* categories[] = {"Apparel", "Beauty", "Digital", "Food", "Home", "Market", "Sports", "Toys"};
    int pipe_fd[2];

    // ایجاد Pipe برای ارتباط بین دسته‌بندی و فروشگاه
    if (pipe(pipe_fd) == -1) {
        perror("Failed to create pipe");
        exit(1);
    }

    for (int i = 0; i < 8; i++) {
        pid_t pid = fork(); // ایجاد فرایند برای هر دسته‌بندی
        if (pid < 0) {
            perror("Fork failed for category");
            exit(1);
        } else if (pid == 0) {
            // کد فرایند فرزند (category)
            close(pipe_fd[0]); // بستن سمت خواندن Pipe در فرزند

            char category_path[512];
            snprintf(category_path, sizeof(category_path), "%s/%s", store_path, categories[i]);

            printf("PID %d create child for %s PID: %d\n", getppid(), categories[i], getpid());

            // فراخوانی handle_category برای پردازش دسته‌بندی
            handle_category(categories[i], category_path, shopping_list, shopping_list_count, pipe_fd[1]);

            close(pipe_fd[1]); // بستن سمت نوشتن Pipe
            exit(0); // خروج از فرایند دسته‌بندی
        } else {
            // والد منتظر می‌ماند تا فرایند فرزند (category) به پایان برسد
            category_pids[i] = pid;
        }
    }

    close(pipe_fd[1]); // بستن سمت نوشتن Pipe در والد

    // خواندن اطلاعات از دسته‌بندی‌ها
    ShoppingItem product;
    printf("Store %s (PID %d): Receiving products from categories...\n", store_name, getpid());
    while (read(pipe_fd[0], &product, sizeof(ShoppingItem)) > 0) {
        printf("Store %s: Received product '%s' with price %.2f\n", store_name, product.product_name, product.price);
    }

    close(pipe_fd[0]); // بستن سمت خواندن Pipe در والد

    // منتظر ماندن برای اتمام همه فرایندهای دسته‌بندی
    for (int i = 0; i < 8; i++) {
        waitpid(category_pids[i], NULL, 0);
    }

    printf("Store %s (PID %d): Finished processing categories.\n", store_name, getpid());
}





int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <temp_file>\n", argv[0]);
        return 1;
    }

    // باز کردن فایل موقت
    FILE* file = fopen(argv[1], "rb");
    if (!file) {
        perror("Failed to open temp file");
        return 1;
    }

    UserInfo user_info = {0};

    // خواندن تعداد محصولات
    fread(&user_info.item_count, sizeof(int), 1, file);

    // تخصیص حافظه برای لیست خرید
    user_info.shopping_list = malloc(user_info.item_count * sizeof(ShoppingItem));
    if (!user_info.shopping_list) {
        perror("Failed to allocate memory");
        fclose(file);
        return 1;
    }

    // خواندن لیست خرید از فایل
    fread(user_info.shopping_list, sizeof(ShoppingItem), user_info.item_count, file);

    // خواندن بودجه
    fread(&user_info.budget, sizeof(double), 1, file);

    // خواندن نام کاربر
    fread(user_info.name, sizeof(user_info.name), 1, file);

    fclose(file);

    // نمایش اطلاعات کاربر
    printf("%s created PID: %d\n", user_info.name, getpid());

    // تبدیل لیست خرید به آرایه‌ای از رشته‌ها (char**)
    char** shopping_list = malloc(user_info.item_count * sizeof(char*));
    for (int i = 0; i < user_info.item_count; i++) {
        shopping_list[i] = user_info.shopping_list[i].product_name;
    }

    pthread_t tid_orders, tid_scores, tid_final;
    sem_init(&sem_orders_done, 0, 0);
    sem_init(&sem_final_done, 0, 0);

    pthread_create(&tid_orders, NULL, orders_thread, NULL);
    pthread_create(&tid_scores, NULL, scores_thread, NULL);
    pthread_create(&tid_final, NULL, final_thread, NULL);

    pid_t store_pids[3]; // شناسه‌های فرایندهای store
    for (int i = 0; i < 3; i++) {
        pid_t pid = fork(); // ایجاد فرایند فرزند برای هر store
        if (pid < 0) {
            perror("Fork failed for store");
            exit(1);
        } else if (pid == 0) {
            // کد فرایند فرزند (store)
            printf("PID %d create child for Store%d PID: %d\n", getppid(), i + 1, getpid());

            // تنظیم مسیر فروشگاه
            char store_path[512];
            snprintf(store_path, sizeof(store_path), "./Dataset/Store%d", i + 1);

            // فراخوانی handle_store
            handle_store((i == 0) ? "Store1" : (i == 1) ? "Store2" : "Store3",
                         store_path, shopping_list, user_info.item_count);

            exit(0); // خروج از فرایند store
        } else {
            // والد منتظر می‌ماند تا فرایند store به پایان برسد
            store_pids[i] = pid;
            waitpid(pid, NULL, 0);
        }
    }

    // منتظر ماندن برای اتمام thread ها
    pthread_join(tid_orders, NULL);
    pthread_join(tid_scores, NULL);
    pthread_join(tid_final, NULL);

    sem_destroy(&sem_orders_done);
    sem_destroy(&sem_final_done);

    free(user_info.shopping_list);
    free(shopping_list);

    return 0;
}