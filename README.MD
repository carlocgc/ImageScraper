# ImageScraper

  

A gui based windows application that downloads media from social platforms.  
It features OAuth2 authentication and multi threaded downloads.  

![Screenshot](/ImageScraper/data/screenshot.png?raw=true)  

## Setup

  

### ![Reddit](https://img.shields.io/badge/Reddit-%23FF4500.svg?style=for-the-badge&logo=Reddit&logoColor=white)

  

Create web app: https://www.reddit.com/prefs/apps  
Add redirect uri for the web app: http://localhost:8080  
Add client id and secret to `/ImageScraper/config.json`  


### ![Tumblr](https://img.shields.io/badge/Tumblr-%2336465D.svg?style=for-the-badge&logo=Tumblr&logoColor=white)

  

Create app: https://www.tumblr.com/oauth/apps  
Add OAuth Consumer Key to `/ImageScraper/config.json`  

### 4Chan

  

No setup required  


## Development

  

Perform the setup steps above using `/ImageScraper/data/config.json`  

Set debugging working directory to `$(TargetDir)` for all configurations and platforms  

  

The latest curl certificate can be obtained from:  
https://curl.se/ca/cacert.pem  