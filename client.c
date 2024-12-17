#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h> // برای کار با thread ها

// ساختار برای هر محصول
typedef struct {
    char product_name[256];
    int quantity;
} ShoppingItem;

// ساختار برای اطلاعات کاربر
typedef struct {
    char name[256];
    ShoppingItem* shopping_list; // اشاره‌گر برای حافظه پویا
    int item_count;              // تعداد محصولات
    double budget;
} UserInfo;


void* orders_thread(void* arg) {
    printf("PID %d create thread for Orders TID: %lu\n", getpid(), pthread_self());
    pthread_exit(NULL);
}

// تابع thread برای Scores
void* scores_thread(void* arg) {
    printf("PID %d create thread for Scores TID: %lu\n", getpid(), pthread_self());
    pthread_exit(NULL);
}

// تابع thread برای Final
void* final_thread(void* arg) {
    printf("PID %d create thread for Final TID: %lu\n", getpid(), pthread_self());
    pthread_exit(NULL);
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


    pthread_t tid_orders, tid_scores, tid_final;

    pthread_create(&tid_orders, NULL, orders_thread, NULL);
    pthread_create(&tid_scores, NULL, scores_thread, NULL);
    pthread_create(&tid_final, NULL, final_thread, NULL);

    // منتظر ماندن برای اتمام thread ها
    pthread_join(tid_orders, NULL);
    pthread_join(tid_scores, NULL);
    pthread_join(tid_final, NULL);

    free(user_info.shopping_list);

    return 0;
}
