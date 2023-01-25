#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>

struct line{
    unsigned long int tag;
    int isValid;
    int age;
};

struct set{
    struct line *records;
};

unsigned long int intlog2(unsigned int x){
    unsigned int y = 0;
    while(x > 1){
        x >>= 1;
        ++y;
    }
    return y;
}

int main (int argc, char **argv){
    int cachesize = atoi(argv[1]);
    char *associativity = argv[2];
    char *replacementpolicy = argv[3];
    int blocksize = atoi(argv[4]);
    char *tracefile = argv[5];

    int numReads = 0;
    int numWrites = 0;
    int numHits = 0;
    int numMisses = 0;

    int PNumReads = 0;
    int PNumWrites = 0;
    int PNumHits = 0;
    int PNumMisses = 0;

    FILE *input = fopen(tracefile, "r");

    char RorW = 'x';
    long unsigned int address;

    int numOfSets = 0;
    int linesPerSet = 0;

    if(strcmp(associativity, "direct") == 0){
        numOfSets = cachesize / blocksize;
        linesPerSet = 1;
    }
    else if(strcmp(associativity, "assoc") == 0){
        numOfSets = 1;
        linesPerSet = cachesize/blocksize;
    }
    else{
        int stringLength = strlen(associativity);
        char character = associativity[stringLength - 1];
        int n = character - '0';
        numOfSets = cachesize / (blocksize * n);
        linesPerSet = n;
    }

    struct set *cache = malloc(numOfSets * sizeof(struct set) + 1);
    for(int i = 0; i < numOfSets; i++){
        cache[i].records = calloc(linesPerSet, linesPerSet * ((2 * sizeof(int)) + (sizeof(unsigned long int))) + 1);
        for(int j = 0; j < linesPerSet; j++){
            cache[i].records[j].tag = 0;
            cache[i].records[j].isValid = 0;
            cache[i].records[j].age = 0;
        }
    }

    struct set *PCache = malloc(numOfSets * sizeof(struct set) + 1);
    for(int i = 0; i < numOfSets; i++){
        PCache[i].records = calloc(linesPerSet, linesPerSet * ((2 * sizeof(int)) + (sizeof(unsigned long int))) + 1);
        for(int j = 0; j < linesPerSet; j++){
            PCache[i].records[j].tag = 0;
            PCache[i].records[j].isValid = 0;
            PCache[i].records[j].age = 0;
        }
    }

    while(RorW != 'e'){
        fscanf(input, "%*x:");
        fscanf(input, "%*c");
        fscanf(input, "%c", &RorW);
        fscanf(input, "%lx", &address);

        if(RorW == 'e'){
            break;
        }

        int blockOffsetNumBits = intlog2(blocksize);
        int setIndexNumBits = intlog2(numOfSets);
        unsigned long int setIndexBits = (address >> blockOffsetNumBits) & ((1 << setIndexNumBits) - 1);

        //Prefetch 0
        struct set currentSet = cache[setIndexBits];
        unsigned long int tagBits = address >> (setIndexNumBits + blockOffsetNumBits);
        
        int isFound = 0;
        for(int i = 0; i < linesPerSet; i++){
            if((currentSet.records[i].tag == tagBits) && (currentSet.records[i].isValid == 1)){
                numHits++;
                isFound = 1;
                if(strcmp(replacementpolicy, "lru") == 0){
                    if((currentSet.records[i].age != 0) && currentSet.records[i].age == (linesPerSet - 1)){
                        for(int j = 0; j < linesPerSet; j++){
                            currentSet.records[j].age++;
                        }
                        currentSet.records[i].age = 0;
                    }
                    else if((currentSet.records[i].age != 0) && currentSet.records[i].age != (linesPerSet - 1)){
                        for(int j = 0; j < linesPerSet; j++){
                            if(currentSet.records[j].age < currentSet.records[i].age){
                                currentSet.records[j].age++;
                            }
                            currentSet.records[i].age = 0;
                        }
                    }
                }
                if(RorW == 'W'){
                    numWrites++;
                }
                break;
            }
        }
        if(isFound == 0){
            numMisses++;
            int isInserted = 0;
            for(int i = 0; i < linesPerSet; i++){
                if(currentSet.records[i].isValid == 0){
                    currentSet.records[i].tag = tagBits;
                    if(RorW == 'R'){
                        numReads++;
                    }
                    if(RorW == 'W'){
                        numReads++;
                        numWrites++;
                    }
                    for(int j = 0; j < linesPerSet; j++){
                        currentSet.records[j].age++;
                    }
                    currentSet.records[i].age = 0;
                    currentSet.records[i].isValid = 1;
                    isInserted = 1;
                    break;
                }
            }
            if(isInserted == 0){
                for(int i = 0; i < linesPerSet; i++){
                    if(currentSet.records[i].age == (linesPerSet - 1)){
                        currentSet.records[i].tag = tagBits;
                        if(RorW == 'R'){
                            numReads++;
                        }
                        if(RorW == 'W'){
                            numReads++;
                            numWrites++;
                        }
                        for(int j = 0; j < linesPerSet; j++){
                            currentSet.records[j].age++;
                        }
                        currentSet.records[i].age = 0;
                        currentSet.records[i].isValid = 1;
                        isInserted = 1;
                        break;
                    }
                }
            }
        }

        //Prefetch 1
        unsigned long int PAddress = (address >> blockOffsetNumBits) + 1;
        unsigned long int PSetIndexBits = PAddress & ((1 << setIndexNumBits) - 1);
        unsigned long int PTagBits = PAddress >> (setIndexNumBits);
        
        struct set PCurrentSet = PCache[setIndexBits];
        struct set nextSet = PCache[PSetIndexBits];

        isFound = 0;

        //HIT
        for(int i = 0; i < linesPerSet; i++){
            if((PCurrentSet.records[i].tag == tagBits) && (PCurrentSet.records[i].isValid == 1)){
                PNumHits++;
                isFound = 1;
                if(strcmp(replacementpolicy, "lru") == 0){
                    if((PCurrentSet.records[i].age != 0) && PCurrentSet.records[i].age == (linesPerSet - 1)){
                        for(int j = 0; j < linesPerSet; j++){
                            PCurrentSet.records[j].age++;
                        }
                        PCurrentSet.records[i].age = 0;
                    }
                    else if((PCurrentSet.records[i].age != 0) && PCurrentSet.records[i].age != (linesPerSet - 1)){
                        for(int j = 0; j < linesPerSet; j++){
                            if(PCurrentSet.records[j].age < PCurrentSet.records[i].age){
                                PCurrentSet.records[j].age++;
                            }
                            PCurrentSet.records[i].age = 0;
                        }
                    }
                }
                if(RorW == 'W'){
                    PNumWrites++;
                }
                break;
            }
        }

        //MISS
        if(isFound == 0){
            PNumMisses++;
            int isInsertedCurrent = 0;
            int isInsertedNext = 0;
            //Insert in current if there is an invalid
            for(int i = 0; i < linesPerSet; i++){
                if(PCurrentSet.records[i].isValid == 0){
                    PCurrentSet.records[i].tag = tagBits;
                    if(RorW == 'R'){
                        PNumReads++;
                    }
                    if(RorW == 'W'){
                        PNumReads++;
                        PNumWrites++;
                    }
                    for(int j = 0; j < linesPerSet; j++){
                        PCurrentSet.records[j].age++;
                    }
                    PCurrentSet.records[i].age = 0;
                    PCurrentSet.records[i].isValid = 1;
                    isInsertedCurrent = 1;
                    break;
                }
            }

            for(int i = 0; i < linesPerSet; i++){
                if(nextSet.records[i].tag == PTagBits && nextSet.records[i].isValid == 1){
                    isInsertedNext = 1;
                }
            }

            //Insert in next if is invalid
            for(int i = 0; i < linesPerSet; i++){
                if(isInsertedNext == 1){
                    break;
                }
                if(nextSet.records[i].isValid == 0){
                    nextSet.records[i].tag = PTagBits;
                    if(RorW == 'R'){
                        PNumReads++;
                    }
                    if(RorW == 'W'){
                        PNumReads++;
                    }
                    for(int j = 0; j < linesPerSet; j++){
                        nextSet.records[j].age++;
                    }
                    nextSet.records[i].age = 0;
                    nextSet.records[i].isValid = 1;
                    isInsertedNext = 1;
                    break;
                }
            }

            //Insert in current if full
            if(isInsertedCurrent == 0){
                for(int i = 0; i < linesPerSet; i++){
                    if(PCurrentSet.records[i].age == (linesPerSet - 1)){
                        PCurrentSet.records[i].tag = tagBits;
                        if(RorW == 'R'){
                            PNumReads++;
                        }
                        if(RorW == 'W'){
                            PNumReads++;
                            PNumWrites++;
                        }
                        for(int j = 0; j < linesPerSet; j++){
                            PCurrentSet.records[j].age++;
                        }
                        PCurrentSet.records[i].age = 0;
                        PCurrentSet.records[i].isValid = 1;
                        isInsertedCurrent = 1;
                        break;
                    }
                }
            }
            //Insert in next if full
            if(isInsertedNext == 0){
                for(int i = 0; i < linesPerSet; i++){
                    if(nextSet.records[i].age == (linesPerSet - 1)){
                        nextSet.records[i].tag = PTagBits;
                        if(RorW == 'R'){
                            PNumReads++;
                        }
                        if(RorW == 'W'){
                            PNumReads++;
                        }
                        for(int j = 0; j < linesPerSet; j++){
                            nextSet.records[j].age++;
                        }
                        nextSet.records[i].age = 0;
                        nextSet.records[i].isValid = 1;
                        isInsertedNext = 1;
                        break;
                    }
                }
            }
        }
    }

    printf("Prefetch 0\nMemory reads: %d\nMemory writes: %d\nCache hits: %d\nCache misses: %d\n", numReads, numWrites, numHits, numMisses);
    printf("Prefetch 1\nMemory reads: %d\nMemory writes: %d\nCache hits: %d\nCache misses: %d\n", PNumReads, PNumWrites, PNumHits, PNumMisses);

    for(int i = 0; i < numOfSets; i++){
        free(cache[i].records);
    }
    for(int i = 0; i < numOfSets; i++){
        free(PCache[i].records);
    }

    free(cache);
    free(PCache);
    return 0;
}