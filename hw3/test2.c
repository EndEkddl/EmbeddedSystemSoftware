#include <unistd.h>
#include <syscall.h>
#include <stdio.h>

struct mystruct2 {
        int x,y;
};

int main(void){
        struct mystruct2 my_st;
        my_st.x = 2018;
        my_st.y = 1593;

        int i = syscall(378, &my_st);
        printf("syscall %d\n",i);

        return 0;
}