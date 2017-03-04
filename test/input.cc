#include <stdio.h>
#include <fstream>
#include <iostream>
using namespace std;
int main()
{
    int a,b;
    ifstream file1;
    file1.open("spsin.txt");
    file1>>a>>b;
    cout<<a<<" "<<b<<endl;
    char *c=(char *)&a,*d=(char *)&b;
    for (int i=0;i<4;i++)
        c[i]=d[i];
    cout<<a<<" "<<b<<endl;
    return 0;
}
