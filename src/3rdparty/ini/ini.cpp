
#include "ini.h"


#define INITIAL_CAPACITY ( 256 )

#ifndef INI_MALLOC
    #define _CRT_NONSTDC_NO_DEPRECATE 
    #define _CRT_SECURE_NO_WARNINGS
    #include <stdlib.h>
    #define INI_MALLOC( ctx, size ) ( malloc( size ) )
    #define INI_FREE( ctx, ptr ) ( free( ptr ) )
#endif

#ifndef INI_MEMCPY
    #define _CRT_NONSTDC_NO_DEPRECATE 
    #define _CRT_SECURE_NO_WARNINGS
    #include <string.h>
    #define INI_MEMCPY( dst, src, cnt ) ( memcpy( dst, src, cnt ) )
#endif 

#ifndef INI_STRLEN
    #define _CRT_NONSTDC_NO_DEPRECATE 
    #define _CRT_SECURE_NO_WARNINGS
    #include <string.h>
    #define INI_STRLEN( s ) ( strlen( s ) )
#endif 

#ifndef INI_STRICMP
    #ifdef _WIN32
        #define _CRT_NONSTDC_NO_DEPRECATE 
        #define _CRT_SECURE_NO_WARNINGS
        #include <string.h>
        #define INI_STRICMP( s1, s2 ) ( stricmp( s1, s2 ) )
    #else                           
        #include <string.h>         
        #define INI_STRICMP( s1, s2 ) ( strcasecmp( s1, s2 ) )        
    #endif
#endif 


struct ini_internal_section_t
    {
    char name[ 32 ];
    char* name_large;
    };


struct ini_internal_property_t
    {
    int section;
    char name[ 32 ];
    char* name_large;
    char value[ 64 ];
    char* value_large;
    };


struct ini_t
    {
    ini_internal_section_t* sections;
    int section_capacity;
    int section_count;

    ini_internal_property_t* properties;
    int property_capacity;
    int property_count;

    void* memctx;
    };


static int ini_internal_property_index( ini_t const* ini, int section, int property )
    {
    int i;
    int p;

    if( ini && section >= 0 && section < ini->section_count )
        {
        p = 0;
        for( i = 0; i < ini->property_count; ++i )
            {
            if( ini->properties[ i ].section == section )
                {
                if( p == property ) return i;
                ++p;
                }
            }
        }

    return INI_NOT_FOUND;
    }


ini_t* ini_create( void* memctx )
    {
    ini_t* ini;

    ini = (ini_t*) INI_MALLOC( memctx, sizeof( ini_t ) );
    ini->memctx = memctx;
    ini->sections = (ini_internal_section_t*) INI_MALLOC( ini->memctx, INITIAL_CAPACITY * sizeof( ini->sections[ 0 ] ) );
    ini->section_capacity = INITIAL_CAPACITY;
    ini->section_count = 1; /* global section */
    ini->sections[ 0 ].name[ 0 ] = '\0'; 
    ini->sections[ 0 ].name_large = 0;
    ini->properties = (ini_internal_property_t*) INI_MALLOC( ini->memctx, INITIAL_CAPACITY * sizeof( ini->properties[ 0 ] ) );
    ini->property_capacity = INITIAL_CAPACITY;
    ini->property_count = 0;
    return ini;
    }


ini_t* ini_load( char const* data, void* memctx )
    {
    ini_t* ini;
    char const* ptr;
    int s;
    char const* start;
    char const* start2;
    int l;

    ini = ini_create( memctx );

    ptr = data;
    if( ptr )
        {
        s = 0;
        while( *ptr )
            {
            /* trim leading whitespace */
            while( *ptr && *ptr <=' ' )
                ++ptr;
            
            /* done? */
            if( !*ptr ) break;

            /* comment */
            else if( *ptr == ';' )
                {
                while( *ptr && *ptr !='\n' )
                    ++ptr;
                }
            /* section */
            else if( *ptr == '[' )
                {
                ++ptr;
                start = ptr;
                while( *ptr && *ptr !=']' && *ptr != '\n' )
                    ++ptr;

                if( *ptr == ']' )
                    {
                    s = ini_section_add( ini, start, (int)( ptr - start) );
                    ++ptr;
                    }
                }
            /* property */
            else
                {
                start = ptr;
                while( *ptr && *ptr !='=' && *ptr != '\n' )
                    ++ptr;

                if( *ptr == '=' )
                    {
                    l = (int)( ptr - start);
                    ++ptr;
                    while( *ptr && *ptr <= ' ' && *ptr != '\n' ) 
                        ptr++;
                    start2 = ptr;
                    while( *ptr && *ptr != '\n' )
                        ++ptr;
                    while( *(--ptr) <= ' ' ) 
                        (void)ptr;
                    ptr++;
                    ini_property_add( ini, s, start, l, start2, (int)( ptr - start2) );
                    }
                }
            }
        }   

    return ini;
    }


int ini_save( ini_t const* ini, char* data, int size )
    {
    int s;
    int p;
    int i;
    int l;
    char* n;
    int pos;

    if( ini )
        {
        pos = 0;
        for( s = 0; s < ini->section_count; ++s )
            {
            n = ini->sections[ s ].name_large ? ini->sections[ s ].name_large : ini->sections[ s ].name;
            l = (int) INI_STRLEN( n );
            if( l > 0 )
                {
                if( data && pos < size ) data[ pos ] = '[';
                ++pos;
                for( i = 0; i < l; ++i )
                    {
                    if( data && pos < size ) data[ pos ] = n[ i ];
                    ++pos;
                    }
                if( data && pos < size ) data[ pos ] = ']';
                ++pos;
                if( data && pos < size ) data[ pos ] = '\n';
                ++pos;
                }

            for( p = 0; p < ini->property_count; ++p )
                {
                if( ini->properties[ p ].section == s )
                    {
                    n = ini->properties[ p ].name_large ? ini->properties[ p ].name_large : ini->properties[ p ].name;
                    l = (int) INI_STRLEN( n );
                    for( i = 0; i < l; ++i )
                        {
                        if( data && pos < size ) data[ pos ] = n[ i ];
                        ++pos;
                        }
                    if( data && pos < size ) data[ pos ] = '=';
                    ++pos;
                    n = ini->properties[ p ].value_large ? ini->properties[ p ].value_large : ini->properties[ p ].value;
                    l = (int) INI_STRLEN( n );
                    for( i = 0; i < l; ++i )
                        {
                        if( data && pos < size ) data[ pos ] = n[ i ];
                        ++pos;
                        }
                    if( data && pos < size ) data[ pos ] = '\n';
                    ++pos;
                    }
                }

            if( pos > 0 )
                {
                if( data && pos < size ) data[ pos ] = '\n';
                ++pos;
                }
            }

        if( data && pos < size ) data[ pos ] = '\0';
        ++pos;

        return pos;
        }

    return 0;
    }


void ini_destroy( ini_t* ini )
    {
    int i;

    if( ini )
        {
        for( i = 0; i < ini->property_count; ++i )
            {
            if( ini->properties[ i ].value_large ) INI_FREE( ini->memctx, ini->properties[ i ].value_large );
            if( ini->properties[ i ].name_large ) INI_FREE( ini->memctx, ini->properties[ i ].name_large );
            }
        for( i = 0; i < ini->section_count; ++i )
            if( ini->sections[ i ].name_large ) INI_FREE( ini->memctx, ini->sections[ i ].name_large );
        INI_FREE( ini->memctx, ini->properties );
        INI_FREE( ini->memctx, ini->sections );
        INI_FREE( ini->memctx, ini );
        }
    }


int ini_section_count( ini_t const* ini )
    {
    if( ini ) return ini->section_count;
    return 0;
    }


char const* ini_section_name( ini_t const* ini, int section )
    {
    if( ini && section >= 0 && section < ini->section_count )
        return ini->sections[ section ].name_large ? ini->sections[ section ].name_large : ini->sections[ section ].name;

    return NULL;
    }


int ini_property_count( ini_t const* ini, int section )
    {
    int i;
    int count;

    if( ini )
        {
        count = 0;
        for( i = 0; i < ini->property_count; ++i )
            {
            if( ini->properties[ i ].section == section ) ++count;
            }
        return count;
        }

    return 0;
    }


char const* ini_property_name( ini_t const* ini, int section, int property )
    {
    int p;

    if( ini && section >= 0 && section < ini->section_count )
        {
        p = ini_internal_property_index( ini, section, property );
        if( p != INI_NOT_FOUND )
            return ini->properties[ p ].name_large ? ini->properties[ p ].name_large : ini->properties[ p ].name;
        }

    return NULL;
    }


char const* ini_property_value( ini_t const* ini, int section, int property )
    {
    int p;

    if( ini && section >= 0 && section < ini->section_count )
        {
        p = ini_internal_property_index( ini, section, property );
        if( p != INI_NOT_FOUND )
            return ini->properties[ p ].value_large ? ini->properties[ p ].value_large : ini->properties[ p ].value;
        }

    return NULL;
    }


int ini_find_section( ini_t const* ini, char const* name, int name_length )
    {
    int i;

    if( ini && name )
        {
        if( name_length <= 0 ) name_length = (int) INI_STRLEN( name );
        for( i = 0; i < ini->section_count; ++i )
            {
            char const* const other = 
                ini->sections[ i ].name_large ? ini->sections[ i ].name_large : ini->sections[ i ].name;
            if( (int) INI_STRLEN( other ) == name_length && INI_STRICMP( name, other ) == 0 )
                return i;
            }
        }

    return INI_NOT_FOUND;
    }


int ini_find_property( ini_t const* ini, int section, char const* name, int name_length )
    {
    int i;
    int c;

    if( ini && name && section >= 0 && section < ini->section_count)
        {
        if( name_length <= 0 ) name_length = (int) INI_STRLEN( name );
        c = 0;
        for( i = 0; i < ini->property_capacity; ++i )
            {
            if( ini->properties[ i ].section == section )
                {
                char const* const other = 
                    ini->properties[ i ].name_large ? ini->properties[ i ].name_large : ini->properties[ i ].name;
                if( (int) INI_STRLEN( other ) == name_length && INI_STRICMP( name, other ) == 0 )
                    return c;
                ++c;
                }
            }
        }

    return INI_NOT_FOUND;
    }


int ini_section_add( ini_t* ini, char const* name, int length )
    {
    ini_internal_section_t* new_sections;
    
    if( ini && name )
        {
        if( length <= 0 ) length = (int) INI_STRLEN( name );
        if( ini->section_count >= ini->section_capacity )
            {
            ini->section_capacity *= 2;
            new_sections = (ini_internal_section_t*) INI_MALLOC( ini->memctx, 
                ini->section_capacity * sizeof( ini->sections[ 0 ] ) );
            INI_MEMCPY( new_sections, ini->sections, ini->section_count * sizeof( ini->sections[ 0 ] ) );
            INI_FREE( ini->memctx, ini->sections );
            ini->sections = new_sections;
            }

        ini->sections[ ini->section_count ].name_large = 0;
        if( length + 1 >= sizeof( ini->sections[ 0 ].name ) )
            {
            ini->sections[ ini->section_count ].name_large = (char*) INI_MALLOC( ini->memctx, (size_t) length + 1 );
            INI_MEMCPY( ini->sections[ ini->section_count ].name_large, name, (size_t) length );
            ini->sections[ ini->section_count ].name_large[ length ] = '\0';
            }
        else
            {
            INI_MEMCPY( ini->sections[ ini->section_count ].name, name, (size_t) length );
            ini->sections[ ini->section_count ].name[ length ] = '\0';
            }

        return ini->section_count++;
        }
    return INI_NOT_FOUND;
    }


void ini_property_add( ini_t* ini, int section, char const* name, int name_length, char const* value, int value_length )
    {
    ini_internal_property_t* new_properties;

    if( ini && name && section >= 0 && section < ini->section_count )
        {
        if( name_length <= 0 ) name_length = (int) INI_STRLEN( name );
        if( value_length <= 0 ) value_length = (int) INI_STRLEN( value );

        if( ini->property_count >= ini->property_capacity )
            {

            ini->property_capacity *= 2;
            new_properties = (ini_internal_property_t*) INI_MALLOC( ini->memctx, 
                ini->property_capacity * sizeof( ini->properties[ 0 ] ) );
            INI_MEMCPY( new_properties, ini->properties, ini->property_count * sizeof( ini->properties[ 0 ] ) );
            INI_FREE( ini->memctx, ini->properties );
            ini->properties = new_properties;
            }
        
        ini->properties[ ini->property_count ].section = section;
        ini->properties[ ini->property_count ].name_large = 0;
        ini->properties[ ini->property_count ].value_large = 0;

        if( name_length + 1 >= sizeof( ini->properties[ 0 ].name ) )
            {
            ini->properties[ ini->property_count ].name_large = (char*) INI_MALLOC( ini->memctx, (size_t) name_length + 1 );
            INI_MEMCPY( ini->properties[ ini->property_count ].name_large, name, (size_t) name_length );
            ini->properties[ ini->property_count ].name_large[ name_length ] = '\0';
            }
        else
            {
            INI_MEMCPY( ini->properties[ ini->property_count ].name, name, (size_t) name_length );
            ini->properties[ ini->property_count ].name[ name_length ] = '\0';
            }

        if( value_length + 1 >= sizeof( ini->properties[ 0 ].value ) )
            {
            ini->properties[ ini->property_count ].value_large = (char*) INI_MALLOC( ini->memctx, (size_t) value_length + 1 );
            INI_MEMCPY( ini->properties[ ini->property_count ].value_large, value, (size_t) value_length );
            ini->properties[ ini->property_count ].value_large[ value_length ] = '\0';
            }
        else
            {
            INI_MEMCPY( ini->properties[ ini->property_count ].value, value, (size_t) value_length );
            ini->properties[ ini->property_count ].value[ value_length ] = '\0';
            }

        ++ini->property_count;
        }
    }


void ini_section_remove( ini_t* ini, int section )
    {
    int p;

    if( ini && section >= 0 && section < ini->section_count )
        {
        if( ini->sections[ section ].name_large ) INI_FREE( ini->memctx, ini->sections[ section ].name_large );
        for( p = ini->property_count - 1; p >= 0; --p ) 
            {
            if( ini->properties[ p ].section == section )
                {
                if( ini->properties[ p ].value_large ) INI_FREE( ini->memctx, ini->properties[ p ].value_large );
                if( ini->properties[ p ].name_large ) INI_FREE( ini->memctx, ini->properties[ p ].name_large );
                ini->properties[ p ] = ini->properties[ --ini->property_count ];
                }
            }

        ini->sections[ section ] = ini->sections[ --ini->section_count  ];
        
        for( p = 0; p < ini->property_count; ++p ) 
            {
            if( ini->properties[ p ].section == ini->section_count )
                ini->properties[ p ].section = section;
            }
        }
    }


void ini_property_remove( ini_t* const ini, int const section, int property )
    {
    int p;

    if( ini && section >= 0 && section < ini->section_count )
        {
        p = ini_internal_property_index( ini, section, property );
        if( p != INI_NOT_FOUND )
            {
            if( ini->properties[ p ].value_large ) INI_FREE( ini->memctx, ini->properties[ p ].value_large );
            if( ini->properties[ p ].name_large ) INI_FREE( ini->memctx, ini->properties[ p ].name_large );
            ini->properties[ p ] = ini->properties[ --ini->property_count  ];
            return;
            }
        }
    }


void ini_section_name_set( ini_t* ini, int section, char const* name, int length )
    {
    if( ini && name && section >= 0 && section < ini->section_count )
        {
        if( length <= 0 ) length = (int) INI_STRLEN( name );
        if( ini->sections[ section ].name_large ) INI_FREE( ini->memctx, ini->sections[ section ].name_large );
        ini->sections[ section ].name_large = 0;
        
        if( length + 1 >= sizeof( ini->sections[ 0 ].name ) )
            {
            ini->sections[ section ].name_large = (char*) INI_MALLOC( ini->memctx, (size_t) length + 1 );
            INI_MEMCPY( ini->sections[ section ].name_large, name, (size_t) length );
            ini->sections[ section ].name_large[ length ] = '\0';
            }
        else
            {
            INI_MEMCPY( ini->sections[ section ].name, name, (size_t) length );
            ini->sections[ section ].name[ length ] = '\0';
            }
        }
    }


void ini_property_name_set( ini_t* ini, int section, int property, char const* name, int length )
    {
    int p;

    if( ini && name && section >= 0 && section < ini->section_count )
        {
        if( length <= 0 ) length = (int) INI_STRLEN( name );
        p = ini_internal_property_index( ini, section, property );
        if( p != INI_NOT_FOUND )
            {
            if( ini->properties[ p ].name_large ) INI_FREE( ini->memctx, ini->properties[ p ].name_large );
            ini->properties[ ini->property_count ].name_large = 0;

            if( length + 1 >= sizeof( ini->properties[ 0 ].name ) )
                {
                ini->properties[ p ].name_large = (char*) INI_MALLOC( ini->memctx, (size_t) length + 1 );
                INI_MEMCPY( ini->properties[ p ].name_large, name, (size_t) length );
                ini->properties[ p ].name_large[ length ] = '\0';
                }
            else
                {
                INI_MEMCPY( ini->properties[ p ].name, name, (size_t) length );
                ini->properties[ p ].name[ length ] = '\0';
                }
            }
        }
    }


void ini_property_value_set( ini_t* ini, int section, int property, char const* value, int length )
    {
    int p;

    if( ini && value && section >= 0 && section < ini->section_count )
        {
        if( length <= 0 ) length = (int) INI_STRLEN( value );
        p = ini_internal_property_index( ini, section, property );
        if( p != INI_NOT_FOUND )
            {
            if( ini->properties[ p ].value_large ) INI_FREE( ini->memctx, ini->properties[ p ].value_large );
            ini->properties[ ini->property_count ].value_large = 0;

            if( length + 1 >= sizeof( ini->properties[ 0 ].value ) )
                {
                ini->properties[ p ].value_large = (char*) INI_MALLOC( ini->memctx, (size_t) length + 1 );
                INI_MEMCPY( ini->properties[ p ].name_large, value, (size_t) length );
                ini->properties[ p ].value_large[ length ] = '\0';
                }
            else
                {
                INI_MEMCPY( ini->properties[ p ].value, value, (size_t) length );
                ini->properties[ p ].name[ length ] = '\0';
                }
            }
        }
    }