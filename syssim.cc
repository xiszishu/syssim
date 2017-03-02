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
volatile bool a;
volatile short thread_num;
volatile short pac[4000];
volatile short c_2_p[200];
volatile short p_2_c[4000];
volatile bool cached[4000];
volatile bool loading[4000];
char *pageaddr[4000];
char *cacheaddr[4000];
long pagesize=sysconf(_SC_PAGESIZE);
void *c,*p,*p1;

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


void pagecopy(char *b,char *a)//copy page from b to a
{
    int i;
    for (i=0;i<4096;i++)
    {
        //cout<<a[i]<<" "<<b[i]<<endl;
        //printf("%d %d\n",a[i],b[i]);
        a[i]=b[i];
    }
}

void evict(int num_c)
{
    int i,j;
    int num_p=c_2_p[num_c];
    pagecopy((char *)c+num_c*pagesize,(char *)p+num_p*pagesize);//copy from cache to page
    c_2_p[num_c]=-1;//invalid the association
    p_2_c[num_p]=-1;
    cached[num_p]=0;
}

int getpagenum(char* addr)//To Do:might use inline
{
    return (int )(addr - (char *)p) >>12;
}
int getoffset(char *addr,char *pagestarter)
{
    return (int)(addr - pagestarter);
}

void loadpage(int num_c,int num_p)
{
    int i,j;
    //cout<<num_c<<" "<<num_p<<endl;
    if (c_2_p[num_c]=-1)
    {
        //cout<<num_c<<" "<<num_p<<"ld"<<endl;
        //printf("%p,%p\n",p,c);
        pagecopy((char *)p+num_p*pagesize,(char *)c+num_c*pagesize);//copy from page to cache
        c_2_p[num_c]=num_p;
        p_2_c[num_p]=num_c;
        //cout<<p_2_c[num_p]<<endl;
        //cached[num_p]=1;
    }
    else
    {
         evict(num_c);//evict the page in the cache
         pagecopy((char *)p+num_p*pagesize,(char *)c+num_c*pagesize);//copy from page to cache
         c_2_p[num_c]=num_p;
         p_2_c[num_p]=num_c;
         //cached[num_p]=1;
    }
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
        num=getpagenum((char *)&a[i]);
        diff=getoffset((char *)&a[i],pageaddr[num]);
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
    for (i = 0; i < n; i++)
    {
        //cout<<i<<endl;
        a[i] = rand();
        //cout<<i<<" "<<a[i]<<endl;
    }
    return 0;
}
int *locate(int *addr)
{
    int num,diff,numc;
    num=getpagenum((char *)addr);
    diff=getoffset((char *)addr,pageaddr[num]);
    if (cached[num])
    {
        numc=p_2_c[num];
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
    a1num=getpagenum((char *)&a[k1]);
    a2num=getpagenum((char *)&a[k2]);

    loading[a1num/4]=1;
    loading[a2num/4]=1;
    a1=locate(&a[k1]);
    a2=locate(&a[k2]);
    //temp = a[k1];
    //a[k1] = a[k2];
    //a[k2] = temp;
    // if (*a1==0)
    // {
    //     printf("zero found:%d\n",k1);
    //     printf("num     addr %p %p\n",&a[k1],a1);
    //     printf("starter addr %p %p\n",pageaddr[a1num],cacheaddr[a1num%4]);
    // }
    // if (*a2==0)
    // {
    //     printf("zero found:%d\n",k2);
    //     printf("num     addr %p %p\n",&a[k2],a2);
    //     printf("starter addr %p %p\n",pageaddr[a2num],cacheaddr[a2num%4]);
    // }
    temp = *a1;
    *a1 = *a2;
    *a2 = temp;
    loading[a1num/4]=0;
    loading[a2num/4]=0;
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
        //cout<<i<<endl;
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
    num=getpagenum((char *)addr);
    diff=getoffset((char *)addr,pageaddr[num]);
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
        pnum=getpagenum((char *)it);
        it=locateH(it);
        while (loading[pnum]);
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
    pnum=getpagenum((char *)it);
    it=locateH(it);
    while (loading[pnum]);
    while(hashArray != NULL && it->key != -1)
    {
        //go to next cell
        //cout<<hashIndex<<endl;
        ++hashIndex;
        //cout<<it->key<<endl;
        //wrap around the table
        hashIndex %= hashtable_size;
        it=hashArray+hashIndex;
        pnum=getpagenum((char *)it);
        it=locateH(it);
        while (loading[pnum]);
    }
    it->data=data;
    it->key=key;
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
            }
            pac[j]=0;
            j+=cachenumber;
        }
        //cout<<maxj<<endl;
        if (!cached[maxj])
        {
            //cout<<maxj<<endl;
            //cout<<maxj<<endl;
            //loading[maxj]=1;//set loading page mark
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
    for (i=0;i<cachenumber;i++)
        cacheaddr[i]=(char *)c+i*pagesize;
    for (i=0;i<pagenumber;i++)
    {
        pageaddr[i]=(char *)p+i*pagesize;
        // printf("%p\n",pageaddr[i]);
        //cout<<pageaddr[i]<<endl;
    }
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
    pthread_create(&thread[0],NULL,sps,p);
    //pthread_create(&thread[0],NULL,hashtable,p);
    struct DataItem *a=(struct DataItem *)p;
    while (thread_num!=threadnumber)
    {
        freshcaches();
        //for (i=0;i<2000;i++)
        //if (a[i]==0)
        //{
        //    printf("zero found %d\n",i);
        //}
        usleep(50);
    }

    pthread_join(thread[0],NULL);
    clear_cache();
    //for (i=0;i<100;i++)
    //    cout<<pac[i]<<endl;
    // a=(struct DataItem *)p;
    // struct DataItem *b=(struct DataItem *)c;
    // for (i=0;i<2000;i++)
    // {
    //     cout<<a[i].key<<" "<<b[i].key<<endl;
    //     cout<<a[i].data<<" "<<b[i].data<<endl;
    //     cout<<"------------------"<<endl;
    // }
    //a=(int *)c;
    //for (i=0;i<2000;i++)
    //    cout<<a[i]<<endl;
    free(p);
    free(c);
    return 0;
}
