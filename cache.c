#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <math.h>

int associativity = 2;          // Associativity of cache
int blocksize_bytes = 32;       // Cache Block size in bytes
int cachesize_kb = 64;          // Cache size in KB
int miss_penalty = 30;
long tagSize = 1;

void
print_usage ()
{
  printf ("Usage: gunzip2 -c <tracefile> | ./cache -a assoc -l blksz -s size -mp mispen\n");
  printf ("  tracefile : The memory trace file\n");
  printf ("  -a assoc : The associativity of the cache\n");
  printf ("  -l blksz : The blocksize (in bytes) of the cache\n");
  printf ("  -s size : The size (in KB) of the cache\n");
  printf ("  -mp mispen: The miss penalty (in cycles) of a miss\n");
  exit (0);
}

typedef struct cache
{
    int valid;
    long tag[32];
    int lru[32];
    int dirty[32];
} cache;


void hexToBin(char input[], char output[])
{
    int i;
    int size;
    size = strlen(input);

    for (i = 0; i < size; i++)
    {
        if (input[i] =='0')
        {
            output[i*4] = '0'; output[i*4+1] = '0'; output[i*4+2] = '0'; output[i*4+3] = '0';
        }
        else if (input[i] =='1')
        {
            output[i*4] = '0'; output[i*4+1] = '0'; output[i*4+2] = '0'; output[i*4+3] = '1';
        }    
        else if (input[i] =='2')
        {
            output[i*4] = '0'; output[i*4+1] = '0'; output[i*4+2] = '1'; output[i*4+3] = '0';
        }    
        else if (input[i] =='3')
        {
            output[i*4] = '0'; output[i*4+1] = '0'; output[i*4+2] = '1'; output[i*4+3] = '1';
        }    
        else if (input[i] =='4')
        {
            output[i*4] = '0'; output[i*4+1] = '1'; output[i*4+2] = '0'; output[i*4+3] = '0';
        }    
        else if (input[i] =='5')
        {
            output[i*4] = '0'; output[i*4+1] = '1'; output[i*4+2] = '0'; output[i*4+3] = '1';
        }    
        else if (input[i] =='6')
        {
            output[i*4] = '0'; output[i*4+1] = '1'; output[i*4+2] = '1'; output[i*4+3] = '0';
        }    
        else if (input[i] =='7')
        {
            output[i*4] = '0'; output[i*4+1] = '1'; output[i*4+2] = '1'; output[i*4+3] = '1';
        }    
        else if (input[i] =='8')
        {
            output[i*4] = '1'; output[i*4+1] = '0'; output[i*4+2] = '0'; output[i*4+3] = '0';
        }
        else if (input[i] =='9')
        {
            output[i*4] = '1'; output[i*4+1] = '0'; output[i*4+2] = '0'; output[i*4+3] = '1';
        }
        else if (input[i] =='a')
        {    
            output[i*4] = '1'; output[i*4+1] = '0'; output[i*4+2] = '1'; output[i*4+3] = '0';
        }
        else if (input[i] =='b')
        {
            output[i*4] = '1'; output[i*4+1] = '0'; output[i*4+2] = '1'; output[i*4+3] = '1';
        }
        else if (input[i] =='c')
        {
            output[i*4] = '1'; output[i*4+1] = '1'; output[i*4+2] = '0'; output[i*4+3] = '0';
        }
        else if (input[i] =='d')
        {    
            output[i*4] = '1'; output[i*4+1] = '1'; output[i*4+2] = '0'; output[i*4+3] = '1';
        }
        else if (input[i] =='e')
        {    
            output[i*4] = '1'; output[i*4+1] = '1'; output[i*4+2] = '1'; output[i*4+3] = '0';
        }
        else if (input[i] =='f')
        {
            output[i*4] = '1'; output[i*4+1] = '1'; output[i*4+2] = '1'; output[i*4+3] = '1';
        }
    }
    output[size*4] = '\0';
}


int main(int argc, char * argv []) {

  long address;
  int loadstore, icount;
  char marker;
  long IC = 0;
  long programIC = 0;
  int i = 0;
  int j = 1;
  int dirty_eviction = 0;
  int load_hits = 0;
  int load_misses = 0;
  int store_hits = 0;
  int store_misses = 0;
  int load = 0;
  int store = 0;
  long et = 0;
  

  // Process the command line arguments
  while (j < argc) {
    if (strcmp ("-a", argv [j]) == 0) {
      j++;
      if (j >= argc)
        print_usage ();
      associativity = atoi (argv [j]);
      j++;
    } else if (strcmp ("-l", argv [j]) == 0) {
      j++;
      if (j >= argc)
        print_usage ();
      blocksize_bytes = atoi (argv [j]);
      j++;
    } else if (strcmp ("-s", argv [j]) == 0) {
      j++;
      if (j >= argc)
        print_usage ();
      cachesize_kb = atoi (argv [j]);
      j++;
    } else if (strcmp ("-mp", argv [j]) == 0) {
      j++;
      if (j >= argc)
        print_usage ();
      miss_penalty = atoi (argv [j]);
      j++;
    } else {
      print_usage ();
    }
  }

  // print out cache configuration
  printf("Cache parameters:\n");
  printf ("Cache Size (KB)\t\t\t%d\n", cachesize_kb);
  printf ("Cache Associativity\t\t%d\n", associativity);
  printf ("Cache Block Size (bytes)\t%d\n", blocksize_bytes);
  printf ("Miss penalty (cyc)\t\t%d\n", miss_penalty);
  printf ("\n");

  //Calculate the tag, index, block offset bits
  int blockOffsetBits = (int)(log(blocksize_bytes)/log(2));
  int set = (int)((cachesize_kb*1024)/(blocksize_bytes*associativity));
  int indexBits = (int)(log(set)/log(2)); 
  int tagBits = 32 - indexBits - blockOffsetBits;
  
  char input[32];
  char output[32];
  char tagBin[32];
  char index[32];
  char blockOffset[32];

  cache newCache[set];
  
  //Initializing cache = null
  for (i=0; i < set; i++)
  {
    for (j=0; j<associativity; j++)
    {
      newCache[i].tag[j] = -1;
      newCache[i].lru[j] = -1;
      newCache[i].dirty[j] = 0;
    }
    newCache[i].valid = 0;
  }


  while (scanf("%c %d %lx %d\n",&marker,&loadstore,&address,&icount) != EOF) {

    //here is where you will want to process your memory accesses  
    //convert address to binary
    sprintf(input, "%lx", address);
    hexToBin(input, output); 
        
    //tag = output[0 - (tagBits-1)];
    //index = output[tagBits -> (tagBits+index-1)];
    for (i = 0; i<tagBits; i++)
    {
      tagBin[i] = output[i];
    }
    tagBin[i] = '\0';

    for (i = 0; i<indexBits; i++)
    {
      index[i] = output[tagBits+i];
    }
    index[i] = '\0';

    //Convert index to dec (indexDec)
    int indexDec = 0;
    int indexi = 0;
    int d = 1;

    for (i= strlen(index) - 1; i>=0; i--)
    {
      indexi = index[i] - '0'; 
      indexDec = indexDec + (d*indexi);
      d = 2 * d;
    }
    
    //Convert tagBin to dec (tagDec)
    int tagDec = 0;
    int tagi = 0;
    d = 1;
    for (i= strlen(tagBin) - 1; i>=0; i--)
    {
      tagi = tagBin[i] - '0'; 
      tagDec = tagDec + (d*tagi);
      d = 2 * d;
    }
    
    //Compare tag
    if (newCache[indexDec].valid == 0)
    {
      newCache[indexDec].valid = 1;
      newCache[indexDec].tag[0] = tagDec;
      newCache[indexDec].lru[0] = 0;

      et = et + miss_penalty;

      if (loadstore == 0)
      {
        //load
        load++;
        load_misses++;
        
      } else {
        //store
        store++;
        store_misses++; 
        newCache[indexDec].dirty[0] = 1;
      }
    }
    else if (newCache[indexDec].valid == 1)
    {
      int hit = 0;
      for (i = 0; i<associativity; i++) 
      {
        if (newCache[indexDec].tag[i] == tagDec)
        {
          // HIT!!!
          hit = 1;
          //update lru
          for (j = 0; j < associativity; j++)
          {
            if ((newCache[indexDec].lru[j] != -1) && (newCache[indexDec].lru[j] < newCache[indexDec].lru[i]))
              newCache[indexDec].lru[j] = newCache[indexDec].lru[j] + 1;
          }
          newCache[indexDec].lru[i] = 0;

          if (loadstore == 0)
          {
            //load
            load++;
            load_hits++;
            
          } else {
            //store
            store++;
            store_hits++;
            newCache[indexDec].dirty[i] = 1;
          }
          break;
        }  
      }

      if (hit == 0)
      {
        // MISS!!!!
        et = et + miss_penalty;

        int found = 0;
        //find the lru block in cache
        for (i = 0; i<associativity; i++)
        {
          if (newCache[indexDec].lru[i] == -1)
          {
            found = 1;
            newCache[indexDec].tag[i] = tagDec;
            //update lru
            for (j = 0; j < i; j++)
            {
                newCache[indexDec].lru[j] = newCache[indexDec].lru[j] + 1;
            }
            newCache[indexDec].lru[i] = 0;
            
            if (loadstore == 0) {
              //load
              load++;
              load_misses++;

            } else {
              //store
              store++;
              store_misses++;
              newCache[indexDec].dirty[i] = 1;
            }  
            break;
          }
        }

        if (found == 0) 
        {
          // all blocks are in use. 
          // replace the highest lru
          int lru1 = 0;
          for (j = 0; j<associativity; j++)
          {
            if (newCache[indexDec].lru[j] == (associativity - 1))
            {
              lru1 = j;
              break;
            }
          }

          newCache[indexDec].tag[lru1] = tagDec;

          for (j = 0; j < associativity; j++)
          {
            newCache[indexDec].lru[j] = newCache[indexDec].lru[j] + 1;
          }
          newCache[indexDec].lru[lru1] = 0;

          if (loadstore == 0) {
            //load
            load++;
            load_misses++;

            if (newCache[indexDec].dirty[lru1] == 1)
            {
              dirty_eviction++;
              et = et+ 2;
              newCache[indexDec].dirty[lru1] = 0;
            } 

          } else {
            //store
            store++;
            store_misses++;
            if (newCache[indexDec].dirty[lru1] == 1)
            {
              dirty_eviction++;
              et = et + 2;
            } else {
                newCache[indexDec].dirty[lru1] = 1;
            }
          }  
        }
      }
    } 
    IC++;
    programIC = programIC + icount;
  }
  
  // Here is where you want to print out stats

  float overall_miss_rate = ((float)load_misses+store_misses)/IC;
  float read_miss_rate = ((float)load_misses)/load;
  float write_miss_rate = ((float)store_misses/store);
  float memory_CPI = ((miss_penalty)*(load_misses+store_misses))/ ((float)programIC);
  float total_CPI = 1 + memory_CPI;
  float AMAT = overall_miss_rate*miss_penalty;

  printf("Simulation results:\n");
  
  //  Use your simulator to output the following statistics.  The 
  //  print statements are provided, just replace the question marks with
  //  your calcuations.
  
  printf("\texecution time %ld cycles\n", et+programIC);
  printf("\tinstructions %ld\n", programIC);
  printf("\tmemory accesses %ld\n", IC);
  printf("\toverall miss rate %.2f\n", overall_miss_rate);
  printf("\tread miss rate %.2f\n", read_miss_rate);
  printf("\tmemory CPI %.2f\n", memory_CPI);
  printf("\ttotal CPI %.2f\n", total_CPI);
  printf("\taverage memory access time %.2f cycles\n",  AMAT);
  printf("dirty evictions %d\n", dirty_eviction);
  printf("load_misses %d\n", load_misses);
  printf("store_misses %d\n", store_misses);
  printf("load_hits %d\n", load_hits);
  printf("store_hits %d\n", store_hits);
}
