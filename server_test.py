# python http server test

import httplib


connections = [httplib.HTTPConnection("localhost:5004") for x in range(0, 102)]

for c in connections:
    c.request("GET","/")
    res = c.getresponse()
    print(str(res.status) + " - " + res.reason)
    
for c in connections:
    c.close()


