/*
 * filter_dynamictext.c -- dynamic text overlay filter
 * Copyright (C) 2011 Ushodaya Enterprises Limited
 * Author: Brian Matherly <pez4brian@yahoo.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <framework/mlt.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/types.h> // for stat()
#include <sys/stat.h>  // for stat()
#include <unistd.h>    // for stat()
#include <time.h>      // for strftime() and gtime()

#define MAX_TEXT_LEN 512


static void get_timecode_str( mlt_filter filter, mlt_frame frame, char* text )
{
	int frames = mlt_frame_get_position( frame );
	double fps = mlt_profile_fps( mlt_service_profile( MLT_FILTER_SERVICE( filter ) ) );
	char tc[12] = "";
	if (fps == 0)
	{
		strncat( text, "-", MAX_TEXT_LEN - strlen(text) - 1 );
	}
	else
	{
		int seconds = frames / fps;
		frames = frames % lrint( fps );
		int minutes = seconds / 60;
		seconds = seconds % 60;
		int hours = minutes / 60;
		minutes = minutes % 60;
		sprintf(tc, "%.2d:%.2d:%.2d:%.2d", hours, minutes, seconds, frames);
		strncat( text, tc, MAX_TEXT_LEN - strlen(text) - 1 );
	}
}

static void get_frame_str( mlt_filter filter, mlt_frame frame, char* text )
{
	int pos = mlt_frame_get_position( frame );
	char s[12];
	snprintf( s, sizeof(s) - 1, "%d", pos );
	strncat( text, s, MAX_TEXT_LEN - strlen(text) - 1 );
}

static void get_filedate_str( mlt_filter filter, mlt_frame frame, char* text )
{
	mlt_producer producer = mlt_producer_cut_parent(mlt_frame_get_original_producer( frame ));
	mlt_properties producer_properties = MLT_PRODUCER_PROPERTIES(producer);
	char* filename = mlt_properties_get( producer_properties, "resource");
	struct stat file_info;

	if( !stat(filename, &file_info))
	{
		struct tm* time_info = gmtime( &(file_info.st_mtime) );
		char date[11] = "";
		strftime( date, 11, "%Y/%m/%d", time_info );
		strncat( text, date, MAX_TEXT_LEN - strlen(text) - 1);
	}
}

/** Perform substitution for keywords that are enclosed in "# #".
*/

static void substitute_keywords(mlt_filter filter, char* result, char* value, mlt_frame frame)
{
	char value_copy[MAX_TEXT_LEN] = "";
	char* keyword = NULL;
	int ct = 0;
	int fromStart = 0;

	// Need to copy the value because strtok will modify it.
	strncpy(value_copy, value, 512);
	keyword = strtok( value_copy, "#" );
	fromStart = ( value_copy[0] == '#' ) ? 1 : 0;

	while ( keyword )
	{
		if ( ct % 2 == fromStart )
		{
			// backslash in front of # suppresses substitution
			if ( keyword[ strlen( keyword ) -1 ] == '\\' )
			{
				// keep characters except backslash
				strncat( result, keyword, strlen( keyword ) -1 );
				strcat( result, "#" );
				ct++;
			}
			else
			{
				strcat( result, keyword );
			}
		}
		else if ( !strcmp( keyword, "timecode" ) )
		{
			get_timecode_str(filter, frame, result);
		}
		else if ( !strcmp( keyword, "frame" ) )
		{
			get_frame_str(filter, frame, result);
		}
		else if ( !strcmp( keyword, "filedate" ) )
		{
			get_filedate_str(filter, frame, result);
		}
		else if ( !strcmp( keyword, "resource" ) )
		{
			// special case: replace #resource# with cut parent resource name
			mlt_producer producer = mlt_producer_cut_parent(mlt_frame_get_original_producer( frame ));
			mlt_properties producer_properties = MLT_PRODUCER_PROPERTIES(producer);
			strncat( result, mlt_properties_get( producer_properties, "resource"), MAX_TEXT_LEN - strlen(result) - 1 );
		}
		else
		{
			// replace keyword with property value from this frame
			mlt_properties frame_properties = MLT_FRAME_PROPERTIES( frame );
			char *frame_value = mlt_properties_get( frame_properties, keyword );
			if( frame_value )
			{
				strncat( result, frame_value, MAX_TEXT_LEN - strlen(result) - 1 );
			}
		}
		keyword = strtok( NULL, "#" );
		ct++;
	}
}

static void apply_filter(mlt_filter filter, mlt_frame frame )
{
	mlt_properties my_properties = MLT_FILTER_PROPERTIES( filter );
	mlt_filter watermark = mlt_properties_get_data( my_properties, "_watermark", NULL );
	mlt_properties watermark_properties = MLT_FILTER_PROPERTIES( watermark );
	char* dynamic_text = mlt_properties_get( my_properties, "argument");

	// Check for keywords in dynamic text
	if ( dynamic_text )
	{
		// Apply keyword substitution before passing the text to the filter.
		char result[512] = "";
		substitute_keywords( filter, result, dynamic_text, frame );
		mlt_properties_set( watermark_properties, "producer.markup", (char*) result );
	}

	// Pass the properties to the watermark filter composite transition
	mlt_properties_set( watermark_properties, "composite.geometry", mlt_properties_get( my_properties, "geometry" ) );

	// Pass the properties to the watermark filter pango producer
	mlt_properties_set( watermark_properties, "producer.font", mlt_properties_get( my_properties, "font" ) );
	mlt_properties_set( watermark_properties, "producer.weight", mlt_properties_get( my_properties, "weight" ) );
	mlt_properties_set( watermark_properties, "producer.fgcolour", mlt_properties_get( my_properties, "fgcolour" ) );
	mlt_properties_set( watermark_properties, "producer.bgcolour", mlt_properties_get( my_properties, "bgcolour" ) );

	// Process the filter
	mlt_filter_process( watermark, frame );
}

/** Get the image.
*/

static int filter_get_image( mlt_frame frame, uint8_t **image, mlt_image_format *format, int *width, int *height, int writable )
{
	// Pop the service
	mlt_filter filter = mlt_frame_pop_service( frame );

	mlt_service_lock( MLT_FILTER_SERVICE( filter ) );

	apply_filter(filter, frame);

	mlt_service_unlock( MLT_FILTER_SERVICE( filter ) );

	// Need to get the image
	return mlt_frame_get_image( frame, image, format, width, height, 1 );
}


/** Filter processing.
*/

static mlt_frame filter_process( mlt_filter filter, mlt_frame frame )
{
	// Push the filter
	mlt_frame_push_service( frame, filter );

	// Register the get image method
	mlt_frame_push_get_image( frame, filter_get_image );

	// Return the frame
	return frame;
}

/** Constructor for the filter.
*/

mlt_filter filter_dynamictext_init( mlt_profile profile, mlt_service_type type, const char *id, void *arg )
{
	// Create the filter
	mlt_filter filter = mlt_filter_new( );
	mlt_filter watermark = mlt_factory_filter( profile, "watermark", "pango:" );

	// Initialise it
	if ( filter && watermark )
	{
		// Get the properties
		mlt_properties properties = MLT_FILTER_PROPERTIES( filter );

		// Store the watermark filter for future use
		mlt_properties_set_data( properties, "_watermark", watermark, 0, ( mlt_destructor )mlt_filter_close, NULL );

		// Assign default values
		mlt_properties_set( properties, "argument", arg ? arg: "#timecode#" );
		mlt_properties_set( properties, "geometry", "0%/0%:100%x100%:100" );
		mlt_properties_set( properties, "font", "Sans 48" );
		mlt_properties_set( properties, "weight", "400" );
		mlt_properties_set( properties, "fgcolour", "0x000000ff" );
		mlt_properties_set( properties, "bgcolour", "0x00000020" );

		// Specify the processing method
		filter->process = filter_process;
	}
	else // filter or watermark failed for some reason
	{
		if( filter )
		{
			mlt_filter_close(filter);
		}

		if( watermark )
		{
			mlt_filter_close(watermark);
		}

		filter = NULL;
	}

	return filter;
}
