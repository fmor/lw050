#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>
#include <curl/curl.h>


//#define DEBUG
#define SWEEX_DEFAULT_USER      "sweex"
#define SWEEX_DEFAULT_PASSWORD  "mysweex"


char* g_username;
char* g_password;
char* g_hostname;
char  g_verbose;

CURL* create_curl();
int lw050_update_conf_and_reboot();
int lw050_upload_conf( const char* filename );
size_t write_callback( char* ptr, size_t size, size_t count, void* date );
void print_help( const char* name );
const char* http_error_str( int err );

int main( int argc, char** argv )
{
    char* filename;
    int c;
    struct stat fileinfo;
   
    filename   = NULL;
    g_username = (char*) SWEEX_DEFAULT_USER;
    g_password = (char*) SWEEX_DEFAULT_PASSWORD;
    g_hostname = NULL; 
    g_verbose  = 0;


    if( argc < 5 )
    {
        print_help( basename(argv[0]) );
        return -1;
    }


    while( (c=getopt(argc, argv, "hp:r:t:u:v")) != -1 )
    {
        switch( c )
        {
            case 'p':
                g_password = optarg;
                break;
            case 'r':
                filename = optarg;
                break;
            case 't':
                g_hostname = optarg;
                break;
            case 'u':
                g_username = optarg;
                break;
            case 'h':
                print_help( basename(argv[0]) );
                return 0;
            case 'v':
                g_verbose = 1;
                break;
            case '?':
                print_help( basename(argv[0]) );
                return -1;
            default:
                print_help( basename(argv[0]) );
                abort();
        }
    }

    if( filename == NULL || g_hostname == NULL )
    {
        print_help( basename(argv[0]) );
        return -1;
    }

    // Test if file exists
    c = stat( filename, &fileinfo );
    if( c != 0 )
    {
        fprintf( stderr, "Cannot stat file '%s'\n", filename );
        return -1;
    }



    curl_global_init( CURL_GLOBAL_ALL );

    c = lw050_upload_conf( filename );
    if( c != 0 )
        goto LBL_FAIL;

    c = lw050_update_conf_and_reboot();
    if( c != 0 )
        goto LBL_FAIL;

    curl_global_cleanup();
    return 0;

LBL_FAIL:
    curl_global_cleanup();
    return -1;
}

void print_help( const char* name )
{
    printf( "%s -t hostname -r config.bin [-u username] [-p password] [-h] [-v]\n", name );
    printf( "-t hostname : hostname or ip address\n" );
    printf( "-u username : if not set use default username\n");
    printf( "-p password : if not set user default password\n");
    printf( "-r filename : restore this configuration\n");
    printf( "-h : print this message\n");
    printf( "-v : be verbose\n" );
}

CURL* create_curl()
{
    CURL* curl;
    curl = curl_easy_init();
    if( curl == NULL )
        return NULL;
    curl_easy_setopt( curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1 );
    curl_easy_setopt( curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC );
    curl_easy_setopt( curl, CURLOPT_USERNAME, g_username );
    curl_easy_setopt( curl, CURLOPT_PASSWORD, g_password );
    if( g_verbose == 0 )
    {
        curl_easy_setopt( curl, CURLOPT_HEADER, 1L );
        curl_easy_setopt( curl, CURLOPT_WRITEDATA, NULL );
        curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, write_callback );
    }

#ifdef DEBUG
    curl_easy_setopt( curl, CURLOPT_VERBOSE, 1L );
#endif
    return curl;
}
const char* http_status_str( int err )
{
    switch( err )
    {
        case 200 : return "[HTTP] OK";
        case 401 : return "[HTTP] UNAUTHORIZED";
    }
    return NULL;
}

int lw050_upload_conf( const char* filename )
{
    static const char* format = "http://%s/incoming/Config.cfg";
    CURL* curl = NULL;
    char* url = NULL;
    int url_sz;
    struct curl_httppost* httppost[2] = { NULL, NULL };
    CURLcode cc;
    long http_response_code = 0;
    
    printf( "Uploading conf... ");
    fflush( stdout );

    
    url_sz = snprintf( NULL, 0, format, g_hostname ) + 1;
    url = (char*) malloc( url_sz );
    sprintf( url, format, g_hostname );

    curl = create_curl();
    if( curl == NULL )
        goto LBL_FAIL;
    
    curl_formadd( &httppost[0], &httppost[1], CURLFORM_COPYNAME, "filename", CURLFORM_CONTENTTYPE, "application/octet-stream", CURLFORM_FILE, filename, CURLFORM_END );
    curl_formadd( &httppost[0], &httppost[1], CURLFORM_COPYNAME, "Upgrade",  CURLFORM_COPYCONTENTS, "Restore", CURLFORM_END );

    curl_easy_setopt( curl, CURLOPT_URL, url );
    curl_easy_setopt( curl, CURLOPT_HTTPPOST, httppost[0] );

    cc = curl_easy_perform( curl );
    if( cc != CURLE_OK )
        goto LBL_FAIL;

    curl_easy_getinfo( curl, CURLINFO_RESPONSE_CODE, &http_response_code );
    if( http_response_code != 200L )
    {
        fprintf( stderr, "%s\n", http_status_str(http_response_code) );
        goto LBL_FAIL;
    }
    
    free( url );
    curl_formfree( httppost[0] );
    curl_easy_cleanup( curl );
    printf( "OK\n");
    return 0;

LBL_FAIL:
    printf( "Fail\n");

    if( httppost[0] )
        curl_formfree( httppost[0] );
    if( curl )
        curl_easy_cleanup( curl );
    if( url )
        free( url );

    return -1;
}


int lw050_update_conf_and_reboot()
{
    static const char* format = "http://%s/userRpm/ConfUpdateTemp.htm";
    CURL* curl = NULL;
    CURLcode r;
    char* url = NULL;
    int url_sz;
    long http_response_code = 0;

    printf( "Reboot... ");
    fflush( stdout );

    url_sz = snprintf( NULL, 0, format, g_hostname ) + 1;
    url = (char*) malloc( url_sz  );
    if( url == NULL )
        goto LBL_FAIL;
    sprintf( url, format, g_hostname );

    curl = create_curl();
    if( curl == NULL )
        goto LBL_FAIL;

    curl_easy_setopt( curl, CURLOPT_URL, url );
    r = curl_easy_perform( curl );
    if( r != CURLE_OK )
        goto LBL_FAIL;
    
    curl_easy_getinfo( curl, CURLINFO_RESPONSE_CODE, &http_response_code );
    if( http_response_code != 200L )
    {
        fprintf( stderr, "%s\n", http_status_str(http_response_code) );
        goto LBL_FAIL;
    }
    
    curl_easy_cleanup( curl );
    free( url );
    printf( "OK\n");
    return 0;

LBL_FAIL:
    printf( "Fail\n");

    if( curl )
        curl_easy_cleanup( curl );
    if( url )
        free( url );
    return -1;
}
size_t write_callback( char* ptr, size_t size, size_t count, void* date )
{
    return size * count;
}
