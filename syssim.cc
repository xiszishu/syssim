/*
  Copyright (C) 2017  Xiao Liu<xiszishu@ucsc.edu>
  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <cstring>
#include <stddef.h>
#include <time.h>
#include <unistd.h>
#include <vector>
#include <fstream>
#define pagenumber 200
#define cachenumber 50
#define threshold 1024
#define threadnumber 1
#define ITEM_COUNT 1000000
#define SIZE 20000

extern "C" {
    extern void mcsim_log_begin();
    extern void mcsim_log_end();
    extern void mcsim_tx_begin();
    extern void mcsim_tx_end();
    extern void mcsim_paccess_begin();
    extern void mcsim_paccess_end();
    extern void mcsim_mem_fence();
    extern void mcsim_skip_instrs_begin();
    extern void mcsim_skip_instrs_end();
}

using namespace std;

//volatile bool a;
volatile short thread_num;
volatile short pac[4000];
volatile short c_2_p[200];
volatile short p_2_c[4000];
volatile bool cached[4000];
volatile bool loading[4000];
volatile bool build_finish;
volatile bool inevict;
unsigned char *pageaddr[4000];
unsigned char *cacheaddr[4000];
long pagesize=sysconf(_SC_PAGESIZE);
void *c,*p,*p1;
//volatile int lastnum[200][1024];

int pcache[100];//store the number of cached bits
int pg[1000];//indicate whether each page is cached or not

int ops,hashtable_size;

struct DataItem
{
    int data;
    int key;
};

struct DataItem* hashArray;
struct DataItem dummyItem;
struct DataItem* item;


bool pagecopy(int pagenum,unsigned char *b,unsigned char *a)//copy page from b to a
{
    int i;
    //int *na,*nb;
    //na=(int *)a;
    //nb=(int *)b;
    //printf("start addr in page load:%p\n",b);
    //for (i=0;i<1024;i++)
    //    cout<<"beforeload "<<na[i]<<" "<<nb[i]<<endl;
    //cout<<"*********"<<endl;
    //for (i=0;i<1024;i++)
    //{
    //na[i]=nb[i];
        //cout<<i<<endl;
        //if (nb[i]==0)
        //{
        //    printf("zero finds when refreshing%d\n",i);
        //    printf("%d %d\n",c_2_p[cachenum],lastnum[c_2_p[cachenum]][i]);
        //}
        //if (loading[cachenum])
        //{
            // cout<<"loading found"<<endl;
    //    return 0;
    //    }
        //lastnum[c_2_p[cachenum]][i]=nb[i];
    //}
    for (i=0;i<4096;i++)
    {
        //cout<<a[i]<<" "<<b[i]<<endl;
        //printf("%d %d\n",a[i],b[i]);
        //cout<<a[i]<<" "<<b[i]<<endl;
        //printf("%d %d\n",a[i],b[i]);
       a[i]=b[i];
       if (loading[pagenum])
       {
           //cout<<"abortion"<<i<<endl;
           return 0;
       }
    }

    //for (i=0;i<1024;i++)
    //    cout<<"afterload "<<na[i]<<" "<<nb[i]<<endl;
    //cout<<"^^^^^^^^^^^^^^^^^^^^^^"<<endl;
    return 1;
}

bool evict(int num_c)
{
    int i,j;
    int num_p=c_2_p[num_c];
    //cout<<"evict: "<<num_c<<" "<<num_p<<endl;
    //printf("evict %d\n",num_p);
    if (!pagecopy(num_p,cacheaddr[num_c],pageaddr[num_p])) return 0;//copy from cache to page
    //printf("finished\n");
    usleep(1);
    if (!loading[num_p])
    {
        inevict=1;
        cached[num_p]=0;
        c_2_p[num_c]=-1;//invalid the association
        p_2_c[num_p]=-1;
        inevict=0;
    }
    return 1;
}

int getpagenum(unsigned char* addr)//To Do:might use inline
{
    return (int )(addr - (unsigned char *)p) >>12;
}
int getoffset(unsigned char *addr,unsigned char *pagestarter)
{
    return (int)(addr - pagestarter);
}

void loadpage(int num_c,int num_p)
{
    int i,j;
    //int *c;
    //int a,b;
    //cout<<num_c<<" "<<num_p<<endl;
    if (c_2_p[num_c]==-1)
    {
        //cout<<num_c<<" "<<num_p<<"ld"<<endl;
        //printf("%p,%p\n",p,c);
        if (!pagecopy(num_p,pageaddr[num_p],cacheaddr[num_c])) return; //copy from page to cache
        c_2_p[num_c]=num_p;
        p_2_c[num_p]=num_c;
        //printf("%p %p\n",(unsigned char *)p+num_p*pagesize,(unsigned char *)c+num_c*pagesize);
        //cout<<"loading page/cache number"<<num_c<<" "<<num_p<<endl;
        //cout<<p_2_c[num_p]<<endl;
        //cout<<<<endl;
        //c=(int *)pageaddr[num_p];
        //for (i=0;i<1024;i++)
        //    lastnum[num_p][i]=c[i];
        cached[num_p]=1;
    }
    else
    {
         //cout<<"dumped: "<<num_c<<endl;
         if (!evict(num_c)) return;//evict the page in the cache
         if (!pagecopy(num_p,pageaddr[num_p],cacheaddr[num_c])) return;//copy from page to cache
         c_2_p[num_c]=num_p;
         p_2_c[num_p]=num_c;
         cached[num_p]=1;
    }
    return;
}

//probably uneffective, every time read, need to decide cached or not

void *test(void* x)
{
    int i,j;
    int *a=(int *)x;
    int num,diff;
    short numc;
    //int *a=(int *)realloc(p,100*sizeof(int));
    //cout<<"I am in"<<endl;
    //printf("%d\n",a[0]);
    for (j=0;j<20000000;j++)
    {
        i=j%2000;
        num=getpagenum((unsigned char *)&a[i]);
        diff=getoffset((unsigned char *)&a[i],pageaddr[num]);
        while (loading[num]);
        if (cached[num])
        {
            //cout<<num<<endl;
            //printf("cache %d\n",*(int *)(cacheaddr[num]+diff));
            numc=p_2_c[num];
            //cout<<num<<" "<<p_2_c[num]<<" "<<numc<<endl;
            (*(int *)(cacheaddr[numc]+diff))++;//page is cached
        }
        else a[i]++;
        pac[num]++;
        //if it is the beginning of the page, see if has been cached into DRAM
        //if (&a[i])
        //printf("%d\n",diff/pagesize);
        //update();
    }
    thread_num++;
    return NULL;
}
//----------------sps--------------------
int build_array(int *a, int n)
{
    int i;
    srand(time(NULL));
    //cout<<sizeof(int)*n<<" "<<pagesize*pagenumber<<endl;
    //cout<<n<<endl;
    //cout<<a[4000]<<endl;
    //printf("start addr in build array:%p\n",a);
    for (i = 0; i < n; i++)
    {
        //cout<<i<<endl;
        a[i] = rand();
        //cout<<i<<" "<<a[i]<<endl;
        //if (a[i]==0) printf("zero found in a %d\n",i);
    }
    build_finish=1;
    //cout<<"last item:"<<getpagenum((unsigned char *)(&a[i-1]))<<endl;

    return 0;
}
int *locate(int *addr)
{
    int num_p,diff,numc;
    int a,b;
    num_p=getpagenum((unsigned char *)addr);
    diff=getoffset((unsigned char *)addr,pageaddr[num_p]);
    //return addr;
    //printf("pageaddr  %p\n",pageaddr[num_p]);
    //cout<<"pageoffset:"<<diff<<endl;
    //cout<<"num_p: "<<num_p<<"diff: "<<diff<<endl;
    //cout<<"cached: "<<cached[num_p]<<"num_p: "<<num_p<<endl;
    //printf("%d %d\n",cached[num_p],num_p);
    //a=cached[num_p];
    if (cached[num_p])
    {
        numc=p_2_c[num_p];
        //if (a!=cached[num_p]) printf("%d\n",num_p);
        //printf("%d\n",cached[num_p]);
        //printf("cacheaddr %p\n",cacheaddr[numc]);
        //cout<<"cached: "<<cached[num_p]<<"num_p: "<<num_p<<"numc: "<<numc<<endl;
        return (int *)(cacheaddr[numc]+diff);
    }
    else return addr;
}
void array_swap(int *a, int n, int i)
{
    int temp, k1, k2;
    int a1num,a2num;
    int *a1,*a2;

    srand(time(NULL)+i*i);
    k1 = rand() % n;
    k2 = rand() % n;


    //cout << "swaps a[" << k1 << "] and a[" << k2 << "]" << endl;
    a1num=getpagenum((unsigned char *)&a[k1]);
    a2num=getpagenum((unsigned char *)&a[k2]);


    while (inevict);
    //cout<<a1num<<" "<<a2num<<endl;
    loading[a1num]=1;
    loading[a2num]=1;
    a1=locate(&a[k1]);
    //cout<<"a1"<<endl;
    a2=locate(&a[k2]);
    //cout<<"a2"<<endl;
    pac[a1num]++;
    pac[a2num]++;
    //temp = a[k1];
    //a[k1] = a[k2];
    //a[k2] = temp;
    //cout<<a1num<<" "<<a2num<<endl;
    //printf("pointer addr: %p %p %d %d %d %d\n",a1,a2,k1,k2,a1num,a2num);
    // if (*a1==0)
    // {
    //     printf("zero found:%d pagenum: %d cache num: %d c_2_p num:%d \n",k1,a1num, p_2_c[a1num],c_2_p[a1num]);
    //     printf("num     addr %p %p\n",&a[k1],a1);
    //     printf("starter addr %p %p\n",pageaddr[a1num],cacheaddr[a1num%4]);
    // }
    // if (*a2==0)
    // {
    //     printf("zero found:%d pagenum: %d cache num: %d c_2_p num:%d \n",k2,a2num, p_2_c[a2num],c_2_p[a2num]);
    //     printf("num     addr %p %p\n",&a[k2],a2);
    //     printf("starter addr %p %p\n",pageaddr[a2num],cacheaddr[a2num%4]);
    // }

    // if (*a1==0)
    // {
    //     //cout<<"a1"<<endl;
    //     printf("a1 found zero %d %d %p\n",k1,*a1,a1);
    //     //printf("found zero %d\n",k1,*a1);
    // }
    // if (*a2==0)
    // {
    //     printf("a2 found zero %d %d %p\n",k2,*a2,a2);
    //     //printf("found zero %d %d\n",k1,*a1);
    // }
    // cout<<"*******before exchange*******"<<endl;

    if ((!cached[a1num])&&(!cached[a2num]))
    {
        //mcsim_paccess_begin();
        temp = *a1;
        *a1 = *a2;
        *a2 = temp;
        //mcsim_paccess_end();
    }
    else if ((!cached[a2num])&&cached[a2num])
    {
        //mcsim_paccess_begin();
        temp = *a1;
        *a1 = *a2;
        //mcsim_paccess_end();
        *a2 = temp;
    }
    else if (cached[a2num]&&(!cached[a2num]))
    {
        temp = *a1;
        //mcsim_paccess_begin();
        *a1 = *a2;
        *a2 = temp;
        //mcsim_paccess_end();
    }
    else
    {
        temp = *a1;
        *a1 = *a2;
        *a2 = temp;
    }

    // if (*a1==0)
    // {
    //     //cout<<"a1"<<endl;
    //     printf("a1 found zero %d %d %p\n",k1,*a1,a1);
    //     //printf("found zero %d\n",k1,*a1);
    // }
    // if (*a2==0)
    // {
    //     printf("a2 found zero %d %d %p\n",k2,*a2,a2);
    //     //printf("found zero %d %d\n",k1,*a1);
    // }
    // cout<<"*********after exchange*********"<<endl;

    loading[a1num]=0;
    loading[a2num]=0;
}
void *sps(void *x)
{
    int i,j;
    int swaps,item_count;
    std::ifstream file1;
    file1.open("sps.txt");
    file1>>item_count>>swaps;
    //cout<<item_count<<" "<<swaps<<endl;
    file1.close();

    //std::vector<int> *array=(static_cast<vector<int>*>(x));
    int *array=(int *)x;
    //cout<<"fault"<<endl;
    //vector<int>::iterator it;
    if (build_array(array, item_count))
    {
        cerr << "Fails to build an array" << endl;
        return NULL;
    }
    // int *a=(int *)x;
    // for (i=0;i<2000;i++)
    //     cout<<a[i]<<endl;
    //cout<<"fault"<<endl;

    for (i = 0; i < swaps; i++)
    {
        //cout<<item_count<<" "<<i<<endl;
        array_swap(array, item_count, i); // swap two entris at a time
    }
    thread_num++;
    return NULL;
}
// void print_array(vector<int>& a, int n, ofstream& file)
// {
//     int i;

//     for (i = 0; i < n; i++)
//         file << a[i] << endl;
// }

//--------------end of sps-------------------
//--------------begin of hashtable-----------
struct DataItem *locateH(struct DataItem *addr)
{
    int num,diff,numc;
    num=getpagenum((unsigned char *)addr);
    diff=getoffset((unsigned char *)addr,pageaddr[num]);
    //cout<<cached[num]<<endl;
    if (cached[num])
    {
        numc=p_2_c[num];
        return (struct DataItem *)(cacheaddr[numc]+diff);
    }
    else return addr;
}

int hashCode(int key)
{
    return key % hashtable_size;
}
struct DataItem *search(int key)
{
    //get the hash
    struct DataItem *it;
    int i,hashIndex = hashCode(key);
    int pnum;
    i=hashIndex;
    //move in array until an empty
    do
    {
        it=hashArray+i;
        pnum=getpagenum((unsigned char *)it);
        it=locateH(it);
        //while (loading[pnum]);
        if ((it->key == key)&&(it->data!=-1))
            return it;
        //go to next cell
        ++i;
        //wrap around the table
        //cout<<it->key<<" "<<it->data<<endl;
        i %= hashtable_size;
    }while(hashIndex!=i);
    return NULL;
}

void deleteH(struct DataItem* item)
{
    int key = item->key;

    struct DataItem temp = *item;
    //assign a dummy item at deleted position
    *item = dummyItem;
    return ;

    // //get the hash
    // int hashIndex = hashCode(key);

    // //move in array until an empty
    // while(hashArray != NULL)
    // {
    //     //cout<<hashIndex<<endl;
    //     if(hashArray[hashIndex].key == key)
    //     {
    //         struct DataItem *temp = &hashArray[hashIndex];
    //         //assign a dummy item at deleted position
    //         hashArray[hashIndex] = dummyItem;
    //         return temp;
    //     }
    //     //go to next cell
    //     ++hashIndex;
    //     //wrap around the table
    //     hashIndex %= SIZE;
    // }
    // return NULL;
}

void display(int size)
{
    int i = 0;
    for(i = 0; i<size; i++)
    {
        printf(" (%d,%d)",hashArray[i].key,hashArray[i].data);
    }

    printf("\n");
}

void insertH(int key,int data)
{
    //struct DataItem item;
    //item.data = data;
    //item->key = key;
    struct DataItem *it;
    int pnum;

    int hashIndex = hashCode(key);
    //i=hashIndex;
    //move in array until an empty
    //cout<<hashIndex<<endl;
    it=hashArray+hashIndex;
    pnum=getpagenum((unsigned char *)it);
    it=locateH(it);
    //while (loading[pnum]);
    while(hashArray != NULL && it->key != -1)
    {
        //go to next cell
        //cout<<hashIndex<<endl;
        ++hashIndex;
        //cout<<it->key<<endl;
        //wrap around the table
        hashIndex %= hashtable_size;
        it=hashArray+hashIndex;
        pnum=getpagenum((unsigned char *)it);
        //loading
        it=locateH(it);
        //while (loading[pnum]);
    }
    while (inevict);
    loading[pnum]=1;
    it->data=data;
    it->key=key;
    loading[pnum]=0;
    //hashArray[hashIndex].data=data;
    //hashArray[hashIndex].key=key;
}

void buildH(int size)
{
    int i;
    srand(time(NULL));
    for (i=1;i<=size;i++)
    {
        hashArray[i-1].key=i-1;
        hashArray[i-1].data=rand();
        //cout<<i<<endl;
    }
}
void *hashtable(void* x)
{
    int i,j,search_key;
    struct DataItem* it;
    std::ifstream file1;
    file1.open("hashtable.txt");
    dummyItem.data=-1;
    dummyItem.key=-1;
    file1>>hashtable_size>>ops;
    hashArray=(struct DataItem*) x;
    buildH(hashtable_size);
    build_finish=1;
    //display(hashtable_size);
    //cout<<hashtable_size<<" "<<ops<<endl;
    for (i=1;i<=ops;i++)
    {
        search_key=rand()%(hashtable_size);
        //cout<<search_key<<endl;
        //cout<<"!!!!!!!!!!!!"<<endl;
        it=search(search_key);
        //cout<<"-----------"<<endl;
        if (it==NULL)
        {
            //cout<<"+++++++"<<endl;
            insertH(search_key,rand());
            //cout<<"+++++++"<<endl;
        }
        else
        {
            //cout<<"ddddddddd"<<endl;
            //cout<<it<<endl;
            //printf("%p\n",it);
            deleteH(it);
        }
        //cout<<i<<endl;
        //display(hashtable_size);
    }
    //display(hashtable_size);
    thread_num++;
    return NULL;
}
//--------------end of hashtable-------------
void freshcaches()
{
    int i,j;
    int maxn,maxj;
    for (i=0;i<cachenumber;i++)
    if (!loading[i])
    {
        maxn=-1;
        j=i;
        while (j<pagenumber)
        {
            if (pac[j]>maxn)
            {
                maxn=pac[j];
                maxj=j;
                //cout<<pac[j]<<endl;
            }
            pac[j]=0;
            j+=cachenumber;

        }
        //cout<<"maxj:"<<maxj<<endl;
        if (!cached[maxj])
        {
            //cout<<maxj<<endl;
            //cout<<maxj<<endl;
            //loading[maxj]=1;//set loading page mark
            //cout<<"loading page"<<i<<" "<<maxj<<endl;
            loadpage(i,maxj);
            //loading[maxj]=0;//release the mark
        }
        //for (j=0;j<4;j++)
        //    pac[i*4+j]=0;
        //memset(&pac[i*4+j],0,sizeof(short)*4);
    }
}
void initiate()
{
    int i,j;
    inevict=0;
    for (i=0;i<pagenumber;i++)
    {
        pac[i]=0;
        cached[i]=0;
        loading[i]=0;
        p_2_c[i]=-1;
    }
    for (i=0;i<cachenumber;i++)
    {
        c_2_p[i]=-1;
    }
    //clear all the registers
    thread_num=0;
    c=calloc(pagenumber/4,pagesize);//allocate pages from DRAM
    p=calloc(pagenumber,pagesize);// allocate pages from NVM
    //int *a=(int *)c;
    //for sps formalization:
    //for (j=0;j<1024*pagenumber/4;j++) a[j]=0;
    //a=(int *)p;
    //for (j=0;j<1024*pagenumber;j++) a[j]=0;

    //for hashtable formalization

    //memset(lastnum,0,sizeof(lastnum));
    //for (i=0;i<pagenumber;i++)
    //    for (j=0;j<1024;j++)
    //        lastnum[i][j]=0;

    for (i=0;i<cachenumber;i++)
    {
        cacheaddr[i]=(unsigned char *)c+i*pagesize;
        //printf("cacheaddr:%d %p\n",i,cacheaddr[i]);
    }
    //printf("cache boundary: %p %p\n", cacheaddr[0], cacheaddr[cachenumber-1]);
    for (i=0;i<pagenumber;i++)
    {
        pageaddr[i]=(unsigned char *)p+i*pagesize;
        //printf("pageaddr:%d %p\n",i,pageaddr[i]);
        // printf("%p\n",pageaddr[i]);
        //cout<<pageaddr[i]<<endl;
    }
    //printf("page boundary: %p %p\n", pageaddr[0], pageaddr[pagenumber-1]);
    build_finish=0;
    //printf("%p,%p\n",p,c);
    //p1=p+50*pagesize;//each process has 50 pages
}
void clear_cache()
{
    int i,j;
    for (i=0;i<cachenumber;i++)
        if (c_2_p[i]!=-1)
        {
            //cout<<i<<endl;
            evict(i);
            //cout<<i<<endl;
        }
}
int main()
{
    int i,j;
    pthread_t thread[2];
    initiate();
    //cout<<"pagesize:"<<pagesize<<endl;
    //cout<<"intsize:"<<sizeof(int)<<endl;
    //cout<<"int num of:"<<pagesize/sizeof(int)<<endl;
    //*******test load page**********//
    // int *a=(int *)p;
    // int *b=(int *)c;
    // for (i=0;i<1000;i++)
    //     a[i]=567;
    // loadpage(0,0);
    // for (i=0;i<1000;i++)
    //     b[i]=999;
    // evict(0);
    // for (i=0;i<1000;i++)
    //      cout<<a[i]<<endl;
    //******end
    //cout<<pagesize<<endl;
    //cout<<pagenumber/2*pagesize<<endl;
    //cout<<((int *)p1-(int *)p)/pagesize<<endl;
    //cout<<p1<<endl;
    //cout<<p<<endl;
    //cout<<(char *)p1<<endl;
    //cout<<(char *)p<<endl;
    //ptrdiff_t dif=(char *)p1-(char *)p;
    //cout<<dif<<endl;
    //cout<<"ssss"<<endl;
    //cout<<sizeof(int)<<endl;
    //pthread_create(&thread[0],NULL,hashtable,p);
    //int *a=(int *)c;
    //int *b;
    //int *d;
    //pthread_create(&thread[0],NULL,sps,p);
    pthread_create(&thread[0],NULL,hashtable,p);
    //struct DataItem *a=(struct DataItem *)p;
    int k=0;
    while (!build_finish);
    while (thread_num!=threadnumber)
    {
        freshcaches();
        //cout<<"k: "<<k<<endl;
        //for (i=0;i<cachenumber;i++)
        //if (c_2_p[i]!=-1)
        //{
            //printf("zero found %d\n",i);
        // a=(int *)cacheaddr[i];
        //    b=(int *)cacheaddr[i];
        //    d=(int *)pageaddr[i];
            //cout<<"$$$$$$$$$$$"<<endl;
            //printf("%p %p\n",a,d);

            //cout<<"loaded page"<<i<<endl;
            //for (j=0;j<1024;j++)
            //if (a[j]==0)
            //{
            //    cout<<i<<" "<<j<<endl;
            //    for (int p=0;p<1024;p++)
            //    {
            //        cout<<"cache "<<b[p]<<" "<<p<<endl;
            //        cout<<"page "<<d[p]<<" "<<p<<endl;
            //    }
            //    return 0;
            //}
        //}
        usleep(50);
        k++;
    }
    //for (i=0;i<2000;i++)
    //    cout<<a[i]<<endl;
    pthread_join(thread[0],NULL);
    clear_cache();
    //for (i=0;i<100;i++)
    //    cout<<pac[i]<<endl;
    //struct DataItem *a=(struct DataItem *)c;
    //struct DataItem *b=(struct DataItem *)p;
    //for (i=0;i<2000;i++)
    //{
    //     cout<<a[i].key<<" "<<b[i].key<<endl;
    //     cout<<a[i].data<<" "<<b[i].data<<endl;
    //     cout<<"------------------"<<endl;
    //}
    //cout<<sizeof(struct DataItem)<<endl;
    //a=(int *)c;
    //for (i=0;i<1024;i++)
    //    cout<<a[i]<<endl;
    //cout<<"k:"<<k<<endl;
    free(p);
    free(c);
    return 0;
}
