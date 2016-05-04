/***************************************************************************
*                                  _   _ ____  _
*  Project                     ___| | | |  _ \| |
*                             / __| | | | |_) | |
*                            | (__| |_| |  _ <| |___
*                             \___|\___/|_| \_\_____|
*
* Copyright (C) 1998 - 2015, Daniel Stenberg, <daniel@haxx.se>, et al.
*
* This software is licensed as described in the file COPYING, which
* you should have received as part of this distribution. The terms
* are also available at https://curl.haxx.se/docs/copyright.html.
*
* You may opt to use, copy, modify, merge, publish, distribute and/or sell
* copies of the Software, and permit persons to whom the Software is
* furnished to do so, under the terms of the COPYING file.
*
* This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
* KIND, either express or implied.
*
***************************************************************************/
/* <DESC>
* Find name from number in hitta.se

* curl and a write callback function is used to download the hitta.se html  
* document (page) into memory.
* 
* The libxml2 html-parser is used get the html document in memory into 
* a created DOM tree and from there is retrieved sets of nodes that matches 
* specified criteria defined as XPath expressions.
* </DESC>
*/

#include "ncidd.h"
#include <ctype.h>
#include <curl/curl.h>

#include <libxml/tree.h>
#include <libxml/HTMLparser.h>
#include <libxml/xpath.h>

#define HITTA_URL "http://www.hitta.se/vem-ringde/%s"
#define HEADER_ACCEPT "Accept:text/html,application/xhtml+xml,application/xml"
#define HEADER_USER_AGENT "User-Agent:Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.17 (KHTML, like Gecko) Chrome/24.0.1312.70 Safari/537.17"

#define HITTA_XPATH_01 "//*[@id=\"item-details\"]/div[2]/div[1]/span/h1/span[1]"     //Person - Singel
#define HITTA_XPATH_02 "//*[@id=\"people\"]/ol/li[1]/div[1]/div[1]/h2/a/span"        //Person - Multi-- + MFL
#define HITTA_XPATH_03 "//*[@id=\"item-details\"]/div[2]/div[1]/h1/span[1]"          //Företag - Singel
#define HITTA_XPATH_04 "//*[@id=\"companies\"]/ol/li[1]/div/div/div[1]/h2/a/span"    //Företag - Multi- + MFL
#define HITTA_XPATH_05 "//*[@id=\"primary-content\"]/h1/span[2]/span"                //Okänt nummer
#define HITTA_MAX    5
#define HITTA_MULTI  " - med flera"
#define HITTA_INTER  "Okänt-Internationellt"
#define HITTA_SECUR  "Spärrat nummer"
#define HITTA_SHORT  "För kort nummer"
#define HITTA_ERROR  "FEL från hitta.se"

#define NATIONAL_PREFX '0'
#define SWE_DEST_CODES \
"-10-11-120-121-122-123-125-13-140-141-142-143-144-150-151-152-155-156-157-158-159-16-\
 171-173-174-175-176-18-19-20-21-200-220-221-222-223-224-225-226-227-23-240-241-243-246-247-248-\
 250-251-253-258-26-270-271-278-280-281-290-291-292-293-294-295-297-300-301-302-303-304-31-\
 320-321-322-325-33-340-345-346-35-36-370-371-372-378-380-381-382-383-390-392-393-40-\
 410-411-413-414-415-416-417-418-42-430-431-433-435-44-451-454-455-456-457-459-46-\
 470-471-472-474-476-477-478-479-480-481-485-486-490-491-492-493-494-495-496-498-499-\
 500-501-502-503-504-505-506-510-511-512-513-514-515-520-521-522-523-524-525-526-528-\
 530-531-532-533-534-54-550-551-552-553-554-555-560-563-564-565-570-571-573-\
 580-581-582-583-584-585-586-587-589-590-591-60-611-612-613-620-621-622-623-624-63-\
 640-642-643-644-645-647-650-651-652-653-657-660-661-662-663-670-671-672-680-682-684-687-\
 690-691-692-693-695-696-70-71-72-73-74-75-76-77-78-8-800-90-900-910-911-912-913-914-915-916-918-\
 920-921-922-923-924-925-926-927-928-929-930-932-933-934-935-939-940-941-942-943-944-\
 950-951-952-953-954-960-961-969-970-971-973-975-976-977-978-980-981-99-"

xmlXPathObjectPtr getNodeSet();
void hittaAlias();
struct responseStruct {
  char *html;
  size_t size;
};
static size_t writeMemoryCallback(void *contents, size_t size, size_t nmemb, void *stream) {
  size_t realsize = size * nmemb;
  struct responseStruct *mem = (struct responseStruct *)stream;

  mem->html = realloc(mem->html, mem->size + realsize + 1);
  if(mem->html == NULL) {
    /* out of memory! */
    // printf("not enough memory (realloc returned NULL)\n");
    return 0;
  }

  memcpy(&(mem->html[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->html[mem->size] = 0;

  return realsize;
}
xmlXPathObjectPtr getNodeSet (xmlDocPtr doc, xmlChar *xpath, char *name) {	
	xmlXPathContextPtr context;
	xmlXPathObjectPtr  result;

	context = xmlXPathNewContext(doc);
	if (context == NULL) {
		// printf("Error in xmlXPathNewContext\n");
    strncpy(name, "Error in xmlXPathNewContext", CIDSIZE - 1);        
		return NULL;
	}
	result = xmlXPathEvalExpression(xpath, context);
	xmlXPathFreeContext(context);
	if (result == NULL) {
		// printf("Error in xmlXPathEvalExpression\n");
    strncpy(name, "Error in xmlXPathEvalExpression", CIDSIZE - 1);        
		return NULL;
	}
	if(xmlXPathNodeSetIsEmpty(result->nodesetval)){
		xmlXPathFreeObject(result);
    // printf("No result\n");
    strncpy(name, "Error xmlXPathNodeSetIsEmpty", CIDSIZE - 1);        
		return NULL;
	}
	return result;
}
void hittaAlias(char *name, char *nmbr) {
  CURL    *curl_handle;
  CURLcode curl_code;

  int      i;
  char     url_buffer[80];
  struct   responseStruct response;
  struct   curl_slist     *http_headers = NULL;

  xmlChar          *xpath;
  xmlChar          *nodeval;
  xmlNodeSetPtr     nodeset;
  xmlXPathObjectPtr result;    

  const char *hittaXpath[HITTA_MAX] = {HITTA_XPATH_01, HITTA_XPATH_02, HITTA_XPATH_03, HITTA_XPATH_04, HITTA_XPATH_05};
  const char *sweDestCodes = SWE_DEST_CODES;
  
  char new_nmbr[CIDSIZE];
  
  /* Remove all non numeric characters from number */
  char  c;
  char *nmbr_old_ptr = nmbr;
  char *nmbr_new_ptr = nmbr;

  while ((c = *nmbr_old_ptr++))
      if (isdigit(c))
          *nmbr_new_ptr++ = c;

  *nmbr_new_ptr = 0; 

  /* Check special & to short numbers */
  if (strcmp(nmbr, "00") == 0) {
    strncpy(name, HITTA_INTER, CIDSIZE - 1);
  }
  else if (strcmp(nmbr, "10") == 0) {
    strncpy(name, HITTA_SECUR, CIDSIZE - 1);        
  }
  else if (strlen(nmbr) < 3) {
    strncpy(name, HITTA_SHORT, CIDSIZE - 1);        
  }
  else {
    /* Split number: insert '-' between Dest.Code & Subscriber number */
    if (nmbr[0] == NATIONAL_PREFX) {
      for (i = 3; i > 0; i--){
        strcpy(new_nmbr, "-");
        if (strlen(nmbr) <= i) continue;
        strncat(new_nmbr, nmbr + 1, i);
        strncat(new_nmbr, "-", 1);
        if (strstr(sweDestCodes, new_nmbr)) {
          new_nmbr[0] = '0';
          if (strlen(new_nmbr) == 4 && strlen(nmbr) > 8 && nmbr[1] == '7' 
          && (nmbr[4] != '0' || (nmbr[3] == '0' && nmbr[4] == '0'))){
            memcpy(&new_nmbr[3], &nmbr[3], 1);
            strncat(new_nmbr, "-", 1);
            i++;;
          }
          strcat(new_nmbr, nmbr + i + 1);
          strcpy(nmbr, new_nmbr);
          break;
        }
      }
    }
    
    /* create url */    
    sprintf(url_buffer, HITTA_URL, nmbr);

    /* allocate memory */    
    response.html = malloc(1);  /* will be grown as needed by the realloc above */
    response.size = 0;          /* no data at this point */

    curl_global_init(CURL_GLOBAL_ALL);

    /* init the curl session */
    curl_handle = curl_easy_init();

    /* check if a handle was received */    
    if (curl_handle) {
      /* set URL to get here */
      curl_easy_setopt(curl_handle, CURLOPT_URL, url_buffer);

      /* provide a user-agent field*/
      curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, HEADER_USER_AGENT);

      /* modify a header curl otherwise adds differently */
      http_headers = curl_slist_append(http_headers, HEADER_ACCEPT);
      
      /* set our custom set of headers */
      curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, http_headers);
     
      /* tell libcurl to follow redirection */
      curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);

      /* disable progress meter, set to 0L to enable and disable debug output */
      curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1L);

      /* send all data to this function  */
      curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, writeMemoryCallback);

      /* pass the 'response' struct to the callback function */
      curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&response);

      /* get it! */
      curl_code = curl_easy_perform(curl_handle);

      /* free the custom headers */
      curl_slist_free_all(http_headers);

      /* cleanup curl stuff */
      curl_easy_cleanup(curl_handle);

      /* we're done with libcurl, so clean it up */
      curl_global_cleanup();

      /* check for curl errors */
      if(curl_code == CURLE_OK) {
        /* parse the html response document in memory and create a DOM tree */
        xmlDocPtr doc = htmlReadDoc((xmlChar*)response.html, NULL, NULL, HTML_PARSE_RECOVER | HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);

         /* free allocated response memory */
        free(response.html);
        
        /* check for parse errors */
        if (doc) {
          /* look for found names */
          for (i=1; i <= HITTA_MAX; i++) {
            xpath = (xmlChar*) hittaXpath[i-1];
            result = getNodeSet(doc, xpath, name);
            if (result) { 
              nodeset = result->nodesetval;
              nodeval = xmlNodeListGetString(doc, nodeset->nodeTab[0]->xmlChildrenNode, 1);
              xmlXPathFreeObject(result);
              
              if (nodeval) {
                strncpy(name, nodeval, CIDSIZE - 1);
                if (i==2 || i==4) strncat(name, HITTA_MULTI, CIDSIZE - strlen(name) - 1 );
                xmlFree(nodeval);
                break;
              }
              else {
                strncpy(name, "Error in xmlNodeListGetString->nodeval", CIDSIZE - 1);        
              }
            }
          }
        }
        else {
          strncpy(name, "Error in htmlReadDoc->No doc", CIDSIZE - 1);        
        }
        xmlFreeDoc(doc);
      }
      else {
        strncpy(name, "Error in curl->Not CURL_OK", CIDSIZE - 1);        
      }
    }
    else {
      strncpy(name, "Error in curl->No curl_handle", CIDSIZE - 1);        
    }
    xmlCleanupParser();
  }    
}