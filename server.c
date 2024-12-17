#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>   // For fork, pipe, and exec
#include <sys/types.h>
#include <sys/wait.h> // For wait

// ساختار برای هر محصول در لیست خرید
typedef struct {
    char product_name[256];
    int quantity;
} ShoppingItem;

// ساختار برای اطلاعات کاربر
typedef struct {
    char name[256];
    ShoppingItem* shopping_list; // اشاره‌گر به حافظه پویا
    int item_count;              // تعداد محصولات در لیست خرید
    double budget;
} UserInfo;

int main() {
    printf("Server is running... Waiting for users.\n");

    while (1) {
        // جمع‌آوری اطلاعات کاربر
        UserInfo user_info = {0};
        char input[256]; // برای گرفتن محصولات ورودی کاربر
        char temp_file[] = "/tmp/user_infoXXXXXX"; // فایل موقت برای داده‌های کاربر

        // گرفتن نام کاربر
        printf("Enter your name: ");
        fgets(user_info.name, sizeof(user_info.name), stdin);
        user_info.name[strcspn(user_info.name, "\n")] = '\0';

        // تخصیص اولیه برای لیست خرید
        user_info.shopping_list = malloc(sizeof(ShoppingItem));
        user_info.item_count = 0;

        // گرفتن لیست خرید
        printf("Hello %s! Enter your shopping list (Product Quantity). Press Enter twice to finish:\n", user_info.name);
        while (1) {
            printf("Enter product and quantity: ");
            fgets(input, sizeof(input), stdin);
            if (strcmp(input, "\n") == 0) {
                break; // پایان لیست خرید با 2 بار Enter
            }

            // اضافه کردن محصول جدید
            char product_name[256];
            int quantity;
            if (sscanf(input, "%s %d", product_name, &quantity) == 2) {
                user_info.item_count++;
                user_info.shopping_list = realloc(user_info.shopping_list, user_info.item_count * sizeof(ShoppingItem));
                strcpy(user_info.shopping_list[user_info.item_count - 1].product_name, product_name);
                user_info.shopping_list[user_info.item_count - 1].quantity = quantity;
            } else {
                printf("Invalid format. Please enter in the format: Product Quantity\n");
            }
        }

        // گرفتن سقف خرید
        printf("Enter your budget (Press Enter to skip): ");
        fgets(input, sizeof(input), stdin);
        if (strcmp(input, "\n") == 0) {
            user_info.budget = -1; // بدون محدودیت قیمت
        } else {
            user_info.budget = atof(input); // تبدیل به double
        }

        // ذخیره اطلاعات کاربر در یک فایل موقت
        int fd = mkstemp(temp_file);
        if (fd == -1) {
            perror("Failed to create temp file");
            exit(1);
        }
        write(fd, &user_info.item_count, sizeof(int)); // ذخیره تعداد محصولات
        write(fd, user_info.shopping_list, user_info.item_count * sizeof(ShoppingItem)); // ذخیره لیست خرید
        write(fd, &user_info.budget, sizeof(double)); // ذخیره بودجه
        write(fd, user_info.name, sizeof(user_info.name)); // ذخیره نام کاربر
        close(fd);

        // ساخت فرایند فرزند
        pid_t pid = fork();
        if (pid < 0) {
            perror("Fork failed");
            exit(1);
        } else if (pid == 0) {
            // فرایند فرزند: اجرای client در یک ترمینال جدید
            char command[512];
            snprintf(command, sizeof(command), "./client %s; exec bash", temp_file);

            execl("/usr/bin/gnome-terminal", "gnome-terminal", "--", "bash", "-c", command, (char *)NULL);
            perror("execl failed");
            exit(1);
        } else {
            // والد
            printf("A new process for %s has been created. Server is ready for the next user.\n", user_info.name);
            free(user_info.shopping_list); // آزادسازی حافظه پویا
        }
    }

    return 0;
}
