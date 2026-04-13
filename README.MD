# ImageScraper

  

A Windows GUI application that downloads media from social platforms  

![Screenshot](/ImageScraper/data/screenshot.png?raw=true)  

## Setup

  

### ![Reddit](https://img.shields.io/badge/Reddit-%23FF4500.svg?style=for-the-badge&logo=Reddit&logoColor=white)

  

Create web app: https://www.reddit.com/prefs/apps  
Add redirect uri for the web app: http://localhost:8080  
Add client id and secret to `/ImageScraper/config.json` (copied from `config.template.json`)  


### ![Tumblr](https://img.shields.io/badge/Tumblr-%2336465D.svg?style=for-the-badge&logo=Tumblr&logoColor=white)

  

Create app: https://www.tumblr.com/oauth/apps  
Add OAuth Consumer Key to `/ImageScraper/config.json` (copied from `config.template.json`)  

### 4Chan

  

No setup required  

## Features

  

* Full OAuth2 flow - User authentication (Reddit)  
* Multi-threaded downloads  
* libcurl - Web requests  
* GLFW - Window rendering  
* Dear ImGui - User interface  
* JSON for Modern C++ (nlohmann) - Serialisation

  

## Development

  

Copy `/ImageScraper/data/config.template.json` to `/ImageScraper/data/config.json` and fill in your credentials  

Set debugging working directory to `$(TargetDir)` for all configurations and platforms  

  

The latest curl certificate can be obtained from:  
https://curl.se/ca/cacert.pem  


## Credit

  

https://github.com/glfw/glfw  
https://github.com/ocornut/imgui  
https://github.com/nlohmann/json  
https://curl.se/libcurl/  