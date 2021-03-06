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

#ifndef WIN32
#include <stdlib.h>
#endif
#include <iostream>
#include <boost/array.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/noncopyable.hpp>
#include <boost/static_assert.hpp>
#include <comma/application/contact_info.h>
#include <comma/application/command_line_options.h>
#include <comma/application/signal_flag.h>
#include <comma/base/types.h>
#include <comma/csv/format.h>

void usage()
{
    std::cerr << "simple udp client: receives udp packets and outputs them on stdout" << std::endl;
    std::cerr << std::endl;
    std::cerr << "rationale: netcat and socat somehow do not work very well with udp" << std::endl;
    std::cerr << std::endl;
    std::cerr << "usage: udp-client <port> [<options>]" << std::endl;
    std::cerr << std::endl;
    std::cerr << "<options>" << std::endl;
    std::cerr << "    --ascii: output timestamp as ascii; default: 64-bit binary" << std::endl;
    std::cerr << "    --binary: output timestamp as 64-bit binary; default" << std::endl;
    std::cerr << "    --delimiter=<delimiter>: if ascii and --timestamp, use this delimiter; default: ','" << std::endl;
    std::cerr << "    --size=<size>: hint of maximum buffer size; default 16384" << std::endl;
    std::cerr << "    --reuse-addr,--reuseaddr: reuse udp address/port" << std::endl;
    std::cerr << "    --timestamp: output packet timestamp (currently just system time)" << std::endl;
    std::cerr << std::endl;
    std::cerr << comma::contact_info << std::endl;
    std::cerr << std::endl;
    exit( 1 );
}

int main( int argc, char** argv )
{
    comma::command_line_options options( argc, argv );
    if( argc < 2 || options.exists( "--help,-h" ) ) { usage(); }
    const std::vector< std::string >& unnamed = options.unnamed( "--ascii,--binary,--reuse-addr,--reuseaddr,--timestamp", "--delimiter,--size" );
    if( unnamed.empty() ) { std::cerr << "udp-client: please specify port" << std::endl; return 1; }
    unsigned short port = boost::lexical_cast< unsigned short >( unnamed[0] );
    bool timestamped = options.exists( "--timestamp" );
    bool binary = !options.exists( "--ascii" );
    char delimiter = options.value( "--delimiter", ',' );
    std::vector< char > packet( options.value( "--size", 16384 ) );
    boost::asio::io_service service;
    boost::asio::ip::udp::socket socket( service );
    socket.open( boost::asio::ip::udp::v4() );
    boost::system::error_code error;
    socket.set_option( boost::asio::ip::udp::socket::broadcast( true ), error );
    if( error ) { std::cerr << "udp-client: failed to set broadcast option on port " << port << std::endl; return 1; }
    if( options.exists( "--reuse-addr,--reuseaddr" ) )
    {
        socket.set_option( boost::asio::ip::udp::socket::reuse_address( true ), error );
        if( error ) { std::cerr << "udp-client: failed to set reuse address option on port " << port << std::endl; return 1; }
    }
    socket.bind( boost::asio::ip::udp::endpoint( boost::asio::ip::udp::v4(), port ), error );
    if( error ) { std::cerr << "udp-client: failed to bind port " << port << std::endl; return 1; }
    comma::signal_flag is_shutdown;

    #ifdef WIN32
    if( binary )
    {
        _setmode( _fileno( stdout ), _O_BINARY );        
    }
    #endif
    
    while( !is_shutdown && std::cout.good() )
    {
        boost::system::error_code error;
        std::size_t size = socket.receive( boost::asio::buffer( packet ), 0, error );
        if( error || size == 0 ) { break; }
        if( timestamped )
        {
            boost::posix_time::ptime timestamp = boost::posix_time::microsec_clock::universal_time();
            BOOST_STATIC_ASSERT( sizeof( boost::posix_time::ptime ) == sizeof( comma::uint64 ) );
            if( binary )
            { 
                static char buf[ sizeof( comma::int64 ) ];
                comma::csv::format::traits< boost::posix_time::ptime, comma::csv::format::time >::to_bin( timestamp, buf );
                std::cout.write( buf, sizeof( comma::int64 ) );
            }
            else
            {
                std::cout << boost::posix_time::to_iso_string( timestamp ) << delimiter;
            }
        }
        std::cout.write( &packet[0], size );
        std::cout.flush();
   }
   return 0;
}
