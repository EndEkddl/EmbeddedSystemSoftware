#include <linux/kernel.h>
#include <linux/uaccess.h>

struct sample{
        int a;
        int b;
};

asmlinkage int sys_newcall3(struct sample *dd){
        struct sample my_st;
        copy_from_user(&my_st, dd, sizeof(my_st));

        int front = my_st.a;
        int back = my_st.b;
        printk("sys_newcall3\n");
        printk("front : %d, back : %d, sum : %d\n", front, back, front+back);

        return 23;
}