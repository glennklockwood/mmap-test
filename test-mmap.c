/*
 *  
 */
#define _XOPEN_SOURCE 600
#define _BSD_SOURCE 1

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <fcntl.h>

/*
 * Prototypes
 */
unsigned long get_cached_page_ct(void *file_mmap, long st_size);
int drop_cached_pages( int fd );
void report_faults( char *string );

/*
 * Principal test
 */
int main( int *argc, char **argv ) {
    struct stat st;
    void *values, *vptr;
    size_t page_size;
    size_t stride;
    int fd, i;

    if ( !(fd = open( argv[1], O_RDWR, 0 )) ) return 1;
    drop_cached_pages( fd );

    fstat( fd, &st );
    get_cached_page_ct( values, st.st_size );

    values = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0 );

    if ( values == MAP_FAILED ) {
        close(fd);
        return 1;
    }

    report_faults( "before reading values" );
    page_size = getpagesize();
/*  stride = 8 * page_size + 1024 + 256 + 64 + 16 + 4 + 1; // readahead works on 3.0.101-0.46 */
/*  stride = 8 * page_size + 1024 + 256 + 64 + 16 + 4 + 2; // readahead doesn't work */
    stride = page_size;
    printf( "page size is %ld bytes, stride is %ld bytes\n", page_size, stride );

/*  madvise( values, st.st_size, MADV_SEQUENTIAL ); */

    for ( vptr = values, i = 0; i < 10; i++ ) 
    { 
        /* play with stride here */
        vptr = values + i * stride;
        printf( "Value %2d (at %ld) is %#04x - ", i, (long)(vptr), *((unsigned char*)(vptr)) );
        report_faults( "after reading value" );
    }

    get_cached_page_ct( values, st.st_size );

    if ( munmap( values, st.st_size ) != 0 )
        return 2;

    close(fd);
    return 0;
}


unsigned long get_cached_page_ct(void *file_mmap, long st_size) {
    size_t page_size = getpagesize();
    size_t page_index;
    unsigned char *mincore_vec;
    unsigned long nblocks = 0;

    mincore_vec = calloc(1, (st_size + page_size - 1) / page_size);
    mincore( file_mmap, st_size, mincore_vec );
    for ( page_index = 0; page_index <= st_size/page_size; page_index++)
        if ( mincore_vec[page_index] & 1 )
            nblocks++;

    free(mincore_vec);
    printf( "%ld pages are in cache\n", nblocks );
    return nblocks;
}

int drop_cached_pages( int fd ) {
    fdatasync(fd);
    return posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);
}

void report_faults( char *string ) {
    struct rusage ru;

    getrusage( RUSAGE_SELF, &ru );

    printf( "%ld major, %ld minor page faults %s\n",
        ru.ru_majflt,
        ru.ru_minflt,
        string );

    return;
}
