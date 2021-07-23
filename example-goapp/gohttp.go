package main

import (
   "net/http"
   "net/http/httputil"
   "time"
   
   "fmt"
)


func main() {
  url := "https://untrusted-root.badssl.com/"
  for true {

   fmt.Printf("Sending an HTTP request\n")
   resp, err:= http.Get(url)
   if err != nil {
	 fmt.Printf("Error while fetching %s: %s\n", url, err)
   } else {
	 b, _ :=httputil.DumpResponse(resp, true)
     fmt.Printf("Succeeded: %s\n", string(b))
   }
   
   time.Sleep(10 * time.Second)

 }

}
