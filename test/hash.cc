#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/time.h>
#include <time.h>
#include <iostream>

#define SIZE 100

using namespace std;

struct DataItem
{
    int data;
    int key;
};

struct DataItem* hashArray;
struct DataItem dummyItem;
struct DataItem* item;

int hashCode(int key) {
   return key % SIZE;
}

struct DataItem *search(int key)
{
   //get the hash
    int i,hashIndex = hashCode(key);
    i=hashIndex;
    //move in array until an empty
    do
    {
        if(hashArray[i].key == key)
            return &hashArray[i];
        //go to next cell
        ++i;
        //wrap around the table
        i %= SIZE;
    }while(hashIndex!=i);
    return NULL;
}

void insertH(int key,int data, struct DataItem* item)
{
   //struct DataItem item = (struct DataItem*) malloc(sizeof(struct DataItem));
   item->data = data;
   item->key = key;

   //get the hash
   //int i,hashIndex = hashCode(key);

   //move in array until an empty or deleted cell
   // while(hashArray != NULL && hashArray[hashIndex].key != -1)
   // {
   //    //go to next cell
   //    ++hashIndex;
   //    //wrap around the table
   //    hashIndex %= SIZE;
   // }
   // hashArray[hashIndex] = item;
}

struct DataItem* deleteH(struct DataItem* item) {
   int key = item->key;

   //get the hash
   int hashIndex = hashCode(key);

   //move in array until an empty
   while(hashArray != NULL)
   {
      if(hashArray[hashIndex].key == key)
      {
          struct DataItem *temp = &hashArray[hashIndex];
         //assign a dummy item at deleted position
         hashArray[hashIndex] = dummyItem;
         return temp;
      }
      //go to next cell
      ++hashIndex;
      //wrap around the table
      hashIndex %= SIZE;
   }
   return NULL;
}

void display()
{
   int i = 0;
   for(i = 0; i<SIZE; i++) {
       //if(hashArray[i] != NULL)
         printf(" (%d,%d)",hashArray[i].key,hashArray[i].data);
         //else
         //printf(" ~~ ");
   }

   printf("\n");
}
void buildH()
{
    int i;
    srand(time(NULL));
    for (i=1;i<=SIZE;i++)
    {
        hashArray[i-1].key=i-1;
        hashArray[i-1].data=rand();
        //cout<<i<<endl;
    }
}
int main()
{
    hashArray = (struct DataItem*) calloc(sizeof(struct DataItem),SIZE);
    //dummyItem = (struct DataItem*) malloc(sizeof(struct DataItem));
    dummyItem.data = -1;
    dummyItem.key = -1;

    buildH();
    // insertH(1, 20);
   //  insertH(2, 70);
   // insertH(42, 80);
   // insertH(4, 25);
   // insertH(12, 44);
   // insertH(14, 32);
   // insertH(17, 11);
   // insertH(13, 78);
   // insertH(37, 97);

   display();
   item = search(37);

   if(item != NULL) {
      printf("Element found: %d\n", item->data);
   } else {
      printf("Element not found\n");
   }

   printf("pointer %p\n",item);
   if (item!=NULL) deleteH(item);
   item = search(37);

   if(item != NULL) {
      printf("Element found: %d\n", item->data);
   } else {
      printf("Element not found\n");
   }
   display();

   return 0;
}
