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

#ifdef WIN32
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#else
#include <sys/time.h>
#include <sys/resource.h>
#endif

#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/thread/thread_time.hpp>
#include <comma/base/exception.h>
#include "./split.h"

namespace comma { namespace csv { namespace applications {

split::split( boost::optional< boost::posix_time::time_duration > period
            , const std::string& suffix
            , const comma::csv::options& csv )
    : ofstream_( boost::bind( &split::ofstream_by_time_, this ) )
    , period_( period )
    , suffix_( suffix )
{
    if( ( csv.has_field( "t" ) || csv.fields.empty() ) && !period ) { COMMA_THROW( comma::exception, "please specify --period" ); }
    if( csv.fields.empty() ) { return; }
    if( csv.binary() ) { binary_.reset( new comma::csv::binary< input >( csv ) ); }
    else { ascii_.reset( new comma::csv::ascii< input >( csv ) ); }
    if( csv.has_field( "block" ) ) { ofstream_ = boost::bind( &split::ofstream_by_block_, this ); }
    else if( csv.has_field( "id" ) ) { ofstream_ = boost::bind( &split::ofstream_by_id_, this ); }
}

void split::write( const char* data, unsigned int size )
{
    mode_ = std::ofstream::out | std::ofstream::binary;
    if( binary_ ) { binary_->get( current_, data ); }
    else { current_.timestamp = boost::get_system_time(); }
    ofstream_().write( data, size );
}

void split::write ( const std::string& line )
{
    mode_ = std::ofstream::out; // quick and dirty
    if( ascii_ ) { ascii_->get( current_, line ); }
    else { current_.timestamp = boost::get_system_time(); }
    std::ofstream& ofs = ofstream_();
    ofs.write( &line[0], line.size() );
    ofs.put( '\n' );
}

std::ofstream& split::ofstream_by_time_()
{
    if( !last_ || current_.timestamp > ( last_->timestamp + *period_ ) )
    {
        file_.close();
        std::string time = boost::posix_time::to_iso_string( current_.timestamp );
        if( time.find_first_of( '.' ) == std::string::npos ) { time += ".000000"; }
        file_.open( ( time + suffix_ ).c_str(), mode_ );
        last_ = current_;
    }
    return file_;
}

std::ofstream& split::ofstream_by_block_()
{
    if( !last_ || last_->block != current_.block )
    {
        file_.close();
        std::string name = boost::lexical_cast< std::string >( current_.block ) + suffix_;
        file_.open( name.c_str(), mode_ );
        last_ = current_;
    }
    return file_;    
}

std::ofstream& split::ofstream_by_id_()
{
    Files::iterator it = files_.find( current_.id );
    if( it == files_.end() )
    {
        #ifdef WIN32
        static unsigned int max_number_of_open_files = 128;
        #else 
        static struct rlimit r;
        static int q = getrlimit( RLIMIT_NOFILE, &r );
        if( q != 0 ) { COMMA_THROW( comma::exception, "getrlimit() failed" ); }
        static unsigned int max_number_of_open_files = static_cast< unsigned int >( r.rlim_cur );
        #endif
        if( files_.size() + 10 > max_number_of_open_files ) { files_.clear(); } // quick and dirty, may be too drastic...
        std::ios_base::openmode mode = mode_;
        if( seen_ids_.find( current_.id ) == seen_ids_.end() ) { seen_ids_.insert( current_.id ); }
        else { mode |= std::ofstream::app; }
        std::string name = boost::lexical_cast< std::string >( current_.id ) + suffix_;
        it = files_.insert( std::make_pair( current_.id, new std::ofstream( name.c_str(), mode ) ) ).first;
    }
    return *it->second;
}

} } } // namespace comma { namespace csv { namespace applications {
