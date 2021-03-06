// This file is part of comma, a generic and flexible library 
// for robotics research.
//
// Copyright (C) 2011 The University of Sydney
//
// comma is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 3 of the License, or (at your option) any later version.
//
// comma is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License 
// for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with comma. If not, see <http://www.gnu.org/licenses/>.

/// @author vsevolod vlaskine

#include <iostream>
#include <string>
#include <boost/scoped_ptr.hpp>
#include <comma/application/command_line_options.h>
#include <comma/application/contact_info.h>
#include <comma/application/signal_flag.h>
#include <comma/base/types.h>
#include <comma/csv/stream.h>
#include <comma/io/stream.h>
#include <comma/name_value/parser.h>
#include <comma/string/string.h>
#include <comma/visiting/traits.h>

static void usage()
{
    std::cerr << std::endl;
    std::cerr << "a quick utility on the popular demand:" << std::endl;
    std::cerr << "join timestamped data from stdin with corresponding" << std::endl;
    std::cerr << "timestamped data from the second input" << std::endl;
    std::cerr << std::endl;
    std::cerr << "timestamps are expected to be fully ordered" << std::endl;
    std::cerr << std::endl;
    std::cerr << "todo: for now only for offline processing" << std::endl;
    std::cerr << "      realtime will work, but not necessarily correctly" << std::endl;
    std::cerr << "      if the bounding stream is delayed more than bounded" << std::endl;
    std::cerr << std::endl;
    std::cerr << "usage: cat a.csv | csv-time-join <how> [<options>] bounding.csv [-] > joined.csv" << std::endl;
    std::cerr << std::endl;
    std::cerr << "<how>" << std::endl;
    std::cerr << "    --by-lower: join by lower timestamp" << std::endl;
    std::cerr << "    --by-upper: join by upper timestamp" << std::endl;
    std::cerr << "                default: --by-lower" << std::endl;
    std::cerr << "    --nearest: join by nearest timestamp" << std::endl;
    std::cerr << "               if 'block' given in --fields, output the whole block" << std::endl;
    //std::cerr << "    --nearest-only: output only the input points nearest to timestamp" << std::endl;
    //std::cerr << "               if 'block' given in --fields, output the whole block" << std::endl;
    std::cerr << std::endl;
    std::cerr << "<input/output options>" << std::endl;
    std::cerr << "    -: if csv-time-join - b.csv, concatenate output as: <stdin><b.csv>" << std::endl;
    std::cerr << "       if csv-time-join b.csv -, concatenate output as: <b.csv><stdin>" << std::endl;
    std::cerr << "       default: csv-time-join - b.csv" << std::endl;
    std::cerr << "    --binary,-b <format>: binary format" << std::endl;
    std::cerr << "    --delimiter,-d <delimiter>: ascii only; default ','" << std::endl;
    std::cerr << "    --fields,-f <fields>: input fields; default: t" << std::endl;
    std::cerr << "    --bound=<seconds>: if present, output only points inside of bound in second as double" << std::endl;
    std::cerr << "    --no-discard (todo): do not discard input points" << std::endl;
    std::cerr << "                         default: discard input points that cannot be" << std::endl;
    std::cerr << "                         consistently timestamped, especially head or tail" << std::endl;
    std::cerr << "    --timestamp-only,--time-only: join only timestamp from the second input" << std::endl;
    std::cerr << "                                  otherwise join the whole line" << std::endl;
    std::cerr << std::endl;
    std::cerr << comma::contact_info << std::endl;
    std::cerr << std::endl;
    exit( -1 );
}

struct Point
{
    boost::posix_time::ptime timestamp;
    unsigned int block;
    Point() : block( 0 ) {}
    Point( const boost::posix_time::ptime& timestamp ) : timestamp( timestamp ), block( 0 ) {}
};

namespace comma { namespace visiting {

template <> struct traits< Point >
{
    template < typename K, typename V > static void visit( const K&, const Point& p, V& v )
    { 
        v.apply( "t", p.timestamp );
        v.apply( "block", p.block );
    }
    
    template < typename K, typename V > static void visit( const K&, Point& p, V& v )
    {
        v.apply( "t", p.timestamp );
        v.apply( "block", p.block );
    }
};
    
} } // namespace comma { namespace visiting {

int main( int ac, char** av )
{
    try
    {
        comma::command_line_options options( ac, av );
        if( options.exists( "--help" ) || options.exists( "-h" ) || ac == 1 ) { usage(); }
        options.assert_mutually_exclusive( "--by-lower,--by-upper,--nearest" );
        bool by_upper = options.exists( "--by-upper" );
        bool nearest = options.exists( "--nearest" );
        bool by_lower = ( options.exists( "--by-lower" ) || !by_upper ) && !nearest;
        //bool nearest_only = options.exists( "--nearest-only" );
        bool timestamp_only = options.exists( "--timestamp-only,--time-only" );
        bool discard = !options.exists( "--no-discard" );
        boost::optional< boost::posix_time::time_duration > bound;
        if( options.exists( "--bound" ) ) { bound = boost::posix_time::microseconds( options.value< double >( "--bound" ) * 1000000 ); }
        comma::csv::options stdin_csv( options, "t" );
        //bool has_block = stdin_csv.has_field( "block" );
        comma::csv::input_stream< Point > stdin_stream( std::cin, stdin_csv );
        std::vector< std::string > unnamed = options.unnamed( "--by-lower,--by-upper,--nearest,--timestamp-only,--time-only,--no-discard", "--binary,-b,--delimiter,-d,--fields,-f,--bound" );
        std::string properties;
        bool bounded_first = true;
        switch( unnamed.size() )
        {
            case 0:
                std::cerr << "csv-time-join: please specify bounding source" << std::endl;
                return 1;
            case 1:
                properties = unnamed[0];
                break;
            case 2:
                if( unnamed[0] == "-" ) { properties = unnamed[1]; }
                else if( unnamed[1] == "-" ) { properties = unnamed[0]; bounded_first = false; }
                else { std::cerr << "csv-time-join: expected either '- <bounding>' or '<bounding> -'; got : " << comma::join( unnamed, ' ' ) << std::endl; return 1; }
                break;
            default:
                std::cerr << "csv-time-join: expected either '- <bounding>' or '<bounding> -'; got : " << comma::join( unnamed, ' ' ) << std::endl;
                return 1;
        }
        comma::io::istream is( comma::split( properties, ';' )[0] );
        comma::name_value::parser parser( "filename" );
        comma::csv::options csv = parser.get< comma::csv::options >( properties );
        if( csv.fields.empty() ) { csv.fields = "t"; }
        comma::csv::input_stream< Point > istream( *is, csv );
        std::pair< std::string, std::string > last;
        std::pair< boost::posix_time::ptime, boost::posix_time::ptime > last_timestamp;
        comma::signal_flag is_shutdown;

        #ifdef WIN32
        if( stdin_csv.binary() ) { _setmode( _fileno( stdout ), _O_BINARY ); }
        #endif
        
        while( !is_shutdown && std::cin.good() && !std::cin.eof() && is->good() && !is->eof() )
        {
            const Point* p = stdin_stream.read();
            if( !p ) { break; }
            bool eof = false;
            while( last_timestamp.first.is_not_a_date_time() || p->timestamp >= last_timestamp.second )
            {
                last_timestamp.first = last_timestamp.second;
                last.first = last.second;
                const Point* q = istream.read();
                if( !q ) { eof = true; break; }
                last_timestamp.second = q->timestamp;
                if( !timestamp_only )
                {
                    if( csv.binary() ) { last.second = std::string( istream.binary().last(), csv.format().size() ); }
                    else { last.second = comma::join( istream.ascii().last(), stdin_csv.delimiter ); }
                }
            }
            if( eof ) { break; }
            if( discard && p->timestamp < last_timestamp.first ) { continue; }
            bool is_first = by_lower || ( nearest && ( p->timestamp - last_timestamp.first ) < ( last_timestamp.second - p->timestamp ) );
            const boost::posix_time::ptime& t = is_first ? last_timestamp.first : last_timestamp.second;
            if( bound && !( ( t - *bound ) <= p->timestamp && p->timestamp <= ( t + *bound ) ) ) { continue; }
            const std::string& s = is_first ? last.first : last.second;
            if( stdin_csv.binary() )
            {
                if( bounded_first ) { std::cout.write( stdin_stream.binary().last(), stdin_csv.format().size() ); }
                if( timestamp_only )
                {
                    static comma::csv::binary< Point > b;
                    std::vector< char > v( b.format().size() );
                    b.put( Point( t ), &v[0] );
                    std::cout.write( &v[0], b.format().size() );
                }
                else
                {
                    std::cout.write( &s[0], s.size() );
                }
                if( !bounded_first ) { std::cout.write( stdin_stream.binary().last(), stdin_csv.format().size() ); }
                std::cout.flush();
            }
            else
            {
                if( bounded_first ) { std::cout << comma::join( stdin_stream.ascii().last(), stdin_csv.delimiter ) << stdin_csv.delimiter; }
                if( timestamp_only ) { std::cout << boost::posix_time::to_iso_string( t ); }
                else { std::cout << s; }
                if( !bounded_first ) { std::cout << stdin_csv.delimiter << comma::join( stdin_stream.ascii().last(), stdin_csv.delimiter ); }
                std::cout << std::endl;
            }
        }
        if( is_shutdown ) { std::cerr << "csv-time-join: interrupted by signal" << std::endl; }
        return 0;     
    }
    catch( std::exception& ex ) { std::cerr << "csv-time-join: " << ex.what() << std::endl; }
    catch( ... ) { std::cerr << "csv-time-join: unknown exception" << std::endl; }
    usage();
}
