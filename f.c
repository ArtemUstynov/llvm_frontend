#include <stdio.h>

//
// Created by Ustynov, Artem on 2019-05-12.
//
int writeln(int x) {
    printf("%d\n", x);
    return 0;
}

int getnum() {
    int i;
    scanf("%d", &i);
    return i;
}

int readln(int * x) {
    return  scanf("%d", x);
}
