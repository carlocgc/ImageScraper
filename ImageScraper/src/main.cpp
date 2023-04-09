/*
Project Name: ImageScraper
Author: Carlo Mongiello
Date: 2023

Description:
The purpose of this program is to scrape media content from a website URL, including social media feeds, and store it locally.
The program allows for asynchronous downloading of all media at the specified URL, with a graphical user interface (GUI) that allows the user to input the URL and configure options.
The program also displays log output to the user.
*/
#include "app/App.h"
#include "async/Promise.h"
#include <iostream>

int main( int argc, char* argv[ ] )
{
    ImageScraper::App app{ };
    int result = app.Run( );
    return result;



    //Promise<int, int> promise;

    //std::thread( [ &promise ]
    //    {
    //        // Do some asynchronous work...
    //        int result = 42;
    //        promise.Resolve( result );
    //    } ).detach( );

    //    Promise<std::string, int> promise2 = promise.Then<std::string, std::string >( [ ]( int value )
    //        {
    //            return std::to_string( value );
    //        } );

    //    promise2.Then<void>( [ ]( const std::string& value )
    //        {
    //            std::cout << "Promise resolved with value: " << value << std::endl;
    //        } );


    //return 0;
}