#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <sys/mman.h>
#include "fat_filelib.h"

#define BLOCK_SIZE 0x200

unsigned int key[4] = {0x0, 0x0, 0x0, 0x0};
unsigned char* mmaped_addr;

//XTEA decrypt QWORD
void xtea_decipher( unsigned int* addr_qword ) 
{
    unsigned int result, first, second;

    result = 0xC6EF3720;
    first = addr_qword[0];
    second = addr_qword[1];
    do
    {
        second -= (first + (0x10 * first ^ (first >> 5))) ^ ( result + key[ ( result >> 11 ) & 3 ] );
        result += 0x61C88647;
        first -= (second + (0x10 * second ^ (second >> 5))) ^ ( result + key[ result & 3 ] );
    }
    while ( result );

    addr_qword[0] = first;
    addr_qword[1] = second;
}



//start_block begin at zero
//src_buf = addr_of_map + start*block*BLOCK_SIZE
void decrypt_blocks( unsigned int start_block, unsigned char* src_buf, unsigned char* dst_buf, unsigned int num_blocks)
{
    unsigned int block_index, key_index, qword_index;
    unsigned int temp_buf[BLOCK_SIZE/sizeof( unsigned int )];
    block_index = 0;
    while( block_index < num_blocks )
    {
        key_index = 0;
        while( key_index < 4 )
        {
            key[key_index] = start_block + block_index;
            key_index++;
        }
        memcpy( temp_buf, src_buf + block_index*BLOCK_SIZE, BLOCK_SIZE );
        qword_index = 0;
//Begin decrypt current block
        while( qword_index < BLOCK_SIZE/8 )
        {
            xtea_decipher( &temp_buf[qword_index*2] );
//Begin compute key for next QWORD
            key_index = 0;
            while( key_index < 4 )
            {
                key[key_index] += key[( key_index + 1 ) % 4];
                key_index++;
            }
//End compute key for next QWORD 
            qword_index++;
        }
//End decrypt current block
        memcpy( dst_buf + block_index*BLOCK_SIZE, temp_buf, BLOCK_SIZE );
        block_index++;        
    }    
}

int media_read( unsigned int start_block, unsigned char* buf, unsigned int num_blocks )
{
    decrypt_blocks( start_block, mmaped_addr + start_block*BLOCK_SIZE, buf, num_blocks);
    return 1;
}



int media_init( const char* filename )
{
    int fd;
    
    if( ( fd = open( filename, O_RDONLY ) ) == -1 )
    {
        printf( "ERROR: Cannot open file %s\n", filename );
        return 0;
    }

    if( ( mmaped_addr = mmap( 0, lseek( fd, 0, SEEK_END ), PROT_READ, MAP_SHARED, fd, 0 ) ) == MAP_FAILED )
    {
        printf( "ERROR: Cannot map file %s\n", filename );
        return 0;
    }
  
    return 1;
}

int media_write()
{
    return 1;
}

// path - "/" - path in FAT FS
// dirname - filename of FAT container 
void read_files_from_directory(const char* path, const char* dirname)
{
    FL_DIR dirstat;
    FL_FILE* file;
    int output_fd;
    unsigned char* data;
    char* filename_buf;


    if (fl_opendir(path, &dirstat))
    {
        struct fs_dir_ent dirent;

        while (fl_readdir(&dirstat, &dirent) == 0)
        {

            if (dirent.is_dir)
            {
                printf( "WARN: Unexpected directory %s in FAT container\n", dirent.filename );
            }
            else
            {
                //FAT_PRINTF(("%s [%d bytes]\r\n", dirent.filename, dirent.size));
                filename_buf = (char*)malloc( strlen( dirent.filename ) + 1 );
                strcpy( filename_buf, "/" );
                strcat( filename_buf, dirent.filename );

                file = fl_fopen( filename_buf, "r" );

                free( filename_buf );

                if( !file )
                {
                    printf( "WARN: Can't open to read file %s in FAT container\n", dirent.filename );
                    continue;
                }
                //file->filelength - длина файла
                data = (unsigned char*)malloc( file->filelength );

                if( fl_fread( data, 1, file->filelength, file ) != file->filelength )
                {
                    printf( "WARN: Can't read file %s in FAT container\n", dirent.filename );
                    free( data );
                    continue;
                }

                if( strlen( dirname ) + strlen( dirent.filename ) + 1 > FILENAME_MAX ) 
                {
                    printf( "WARN: Too long filename: %s/%s\n", dirname, dirent.filename );
                    free( data );
                    continue;
                }

                filename_buf = (char*)malloc( strlen( dirname ) + strlen( dirent.filename ) + 1 );
                strcpy( filename_buf, dirname );
                strcat( filename_buf, "/" );
                strcat( filename_buf, dirent.filename );

                output_fd = open( filename_buf, O_WRONLY | O_CREAT );

                if( output_fd == -1 )
                {
                    printf( "ERROR: Can't open file %s\n", filename_buf );
                    free( filename_buf );
                    free( data );
                    continue;
                }
                
                if( write (output_fd, data, file->filelength ) != file->filelength )
                {
                    printf( "ERROR: Can't write file %s\n", filename_buf );
                }
                close( output_fd );

                free( filename_buf );
                free( data );

                // Close file
                fl_fclose(file);

            }
        }

        fl_closedir(&dirstat);
    }
    else
    {
        printf( "ERROR: Can't open dir %s in FAT container\n", path );
    }

}

void print_help(char* progname)
{
    printf( "usage: %s -f|--srcfile fat_container -d|--dstdir destination_directory\n\t -h|--help\n", progname );
}

int main( int argc, char* argv[] )
{
    char* dstdir = NULL;
    char* srcfile = NULL;
    char* currdir = NULL;

    int currdir_len;
                          
    const char* short_options = "hf:d:";

    const struct option long_options[] = {
        { "help", no_argument, NULL, 'h' },
        { "srcfile", required_argument, NULL, 'f' },
        { "dstdir", required_argument, NULL, 'd' },
        { NULL, 0, NULL, 0 }
    };

    int result;

    while ( ( result = getopt_long( argc, argv, short_options, long_options, NULL ) ) != -1 )
    {
    switch( result )
        {
            case 'h': 
            {
                print_help( argv[0] );		
                exit( EXIT_SUCCESS );
            }
            case 'd': 
            {
                dstdir = optarg;
		            break;
            }
            case 'f': 
            {
	              srcfile = optarg;
		            break;
            }
	    default: 
            {
                printf( "Try %s -h|--help\n", argv[0] );
                exit( EXIT_FAILURE );
            }
        }
    }
    if( !srcfile || !dstdir )
    {
        printf( "Try %s -h|--help\n", argv[0] );
        exit( EXIT_FAILURE );
    }
    // Initialise media
    if( !media_init( srcfile ) )
    {
        exit( EXIT_FAILURE );
    }

    // Initialise File IO Library
    fl_init();

    // Attach media access functions to library
    if (fl_attach_media(media_read, 0) != FAT_INIT_OK)
    {
        printf( "ERROR: Media attach failed\n" );
        exit( EXIT_FAILURE );
    }

    //libworker.so put files in / only
    read_files_from_directory( "/", dstdir );

    fl_shutdown();

//TODO: media_finalize

    exit( EXIT_SUCCESS );
}
















