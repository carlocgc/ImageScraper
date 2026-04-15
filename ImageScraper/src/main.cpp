/*
Project Name: ImageScraper
Copyright (c) 2023 Carlo Mongiello
Developed by Carlo Mongiello
See LICENSE.txt for copyright and licensing details (standard MIT License).
*/

#include "app/App.h"
#include <iostream>

int main( int argc, char* argv[ ] )
{
    int result = EXIT_FAILURE;

    {
        ImageScraper::App app{ };
        result = app.Run( );
    }

    return result;
}