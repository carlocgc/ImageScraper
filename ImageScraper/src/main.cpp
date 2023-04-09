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
#include <iostream>

int main( int argc, char* argv[ ] )
{
    ImageScraper::App app{ };
    int result = app.Run( );
    return result;
}