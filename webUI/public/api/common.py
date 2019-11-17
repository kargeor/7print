from os import path, environ
import sys
import cgi
#import cgitb
#cgitb.enable()

def getUploadDir():
  return path.abspath(path.dirname(path.abspath(__file__)) + '/../../../uploads')

def getConfigDir():
  return path.abspath(path.dirname(path.abspath(__file__)) + '/../../../config')

def readConfig(name):
  if ('/' in name) or ('.' in name):
    return 'ERROR'
  path = getConfigDir() + '/' + name
  with open(path, 'r') as content_file:
    content = content_file.read()
  return content

def notAuth():
  print """\
HTTP/1.1 403 Forbidden
Content-Type: text/html\n
<html>
<body>
Error: Not Authorized
</body>
</html>
"""
  sys.exit(0)

def getCookieVal(name):
  if environ.has_key('HTTP_COOKIE'):
    for cookie in environ['HTTP_COOKIE'].split(';'):
      (k,v) = cookie.split('=', 1)
      if k == name:
        return v

def requireAuth():
  # Accept API key header or cookie
  # Use:
  # echo -n "The quick brown fox jumps over the lazy dog" | shasum -a 256
  # to generate
  apiKey = readConfig('api_key')
  if getCookieVal('api_key') == apiKey:
    return
  if environ.has_key('HTTP_X_API_KEY'):
    if environ['HTTP_X_API_KEY'] == apiKey:
      return
  notAuth()
