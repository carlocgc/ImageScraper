# ImageScraper

  

A gui based windows application that downloads media from social platforms.
It features OAuth2 authentication and multi threaded downloads.

## Setup


### Development


Set debugging working directory to `$(TargetDir)` for all configurations and platforms
  

### Reddit



Create web app: https://www.reddit.com/prefs/apps  
Add redirect uri for the web app: http://localhost:8080  
Add client id and secret to `/ImageScraper/data/config.json`  


### Tumblr

  

Create app: https://www.tumblr.com/oauth/apps  
Add OAuth Consumer Key to `/ImageScraper/data/config.json`  


### Discord

  

Create app: https://discord.com/developers/applications  
Add client id and secret to `/ImageScraper/data/config.json`  


### 4Chan

  

No setup required  