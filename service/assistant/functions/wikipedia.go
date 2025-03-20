// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package functions

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"log"
	"net/http"
	"net/url"
	"strings"

	"github.com/honeycombio/beeline-go"
	"github.com/pebble-dev/bobby-assistant/service/assistant/quota"
	"google.golang.org/genai"
)

type WikiRequest struct {
	Wiki            string `json:"wiki"`
	Query           string `json:"article_name"`
	CompleteArticle bool   `json:"complete_article"`
}

type WikiResponse struct {
	Results string `json:"results"`
}

var urlMap = map[string]string{
	"wikipedia":  "https://en.wikipedia.org/",
	"bulbapedia": "https://bulbapedia.bulbagarden.net/",
}

func init() {
	registerFunction(Registration{
		Definition: genai.FunctionDeclaration{
			Name:        "wikipedia",
			Description: "Look up the content of a single named wiki article, from Wikipedia or topic-specific wikis like Bulbapedia. Never say the wiki page didn't have the information needed without first trying to fetch the complete article.",
			Parameters: &genai.Schema{
				Type:     genai.TypeObject,
				Nullable: false,
				Properties: map[string]*genai.Schema{
					"wiki": {
						Type:        genai.TypeString,
						Description: "The Wiki to search.",
						Nullable:    false,
						Enum:        []string{"wikipedia", "bulbapedia"},
					},
					"article_name": {
						Type:        genai.TypeString,
						Description: "The name of the article to look up",
						Nullable:    false,
					},
					"complete_article": {
						Type:        genai.TypeBoolean,
						Description: "Whether to return the complete article or just the summary. Prefer to fetch only the summary. If the summary didn't have the information you expected, you can try again with the complete article.",
						Nullable:    false,
					},
				},
				Required: []string{"wiki", "article_name"},
			},
		},
		Fn:                        queryWiki,
		Thought:                   queryWikiThought,
		RedactOutputInChatHistory: true,
		InputType:                 WikiRequest{},
	})
}

func queryWikiThought(i interface{}) string {
	args := i.(*WikiRequest)
	if args.Query == "" {
		return "Looking it up..."
	}
	return fmt.Sprintf("Looking up %q...", args.Query)
}

func queryWiki(ctx context.Context, quotaTracker *quota.Tracker, args interface{}) interface{} {
	req := args.(*WikiRequest)
	if _, ok := urlMap[req.Wiki]; !ok {
		return Error{Error: "Unknown wiki: " + req.Wiki}
	}
	results, err := queryWikiInternal(ctx, req.Wiki, req.Query, req.CompleteArticle, true)
	if err != nil {
		return Error{Error: err.Error()}
	}
	return &WikiResponse{
		Results: results,
	}
}

func queryWikiInternal(ctx context.Context, wiki, query string, completeArticle, allowSearch bool) (string, error) {
	ctx, span := beeline.StartSpan(ctx, "query_wiki")
	defer span.Send()
	span.AddField("title", query)
	log.Printf("Looking up %s article: %q (complete: %t)\n", wiki, query, completeArticle)
	qs := url.QueryEscape(query)
	u := urlMap[wiki] + "w/api.php?action=query&prop=revisions&rvprop=content&format=xml&titles=" + qs + "&rvslots=main"
	if !completeArticle {
		u += "&rvsection=0"
	}
	request, err := http.NewRequestWithContext(ctx, "GET", u, nil)
	if err != nil {
		return "", err
	}
	request.Header.Set("User-Agent", "Bobby/0.1 (https://github.com/pebble-dev/bobby-assistant)")
	response, err := http.DefaultClient.Do(request)
	if err != nil {
		return "", err
	}
	defer response.Body.Close()
	if response.StatusCode != http.StatusOK {
		content, err := io.ReadAll(response.Body)
		if err != nil {
			return "", err
		}
		return "", fmt.Errorf("%s query failed: %s", wiki, content)
	}
	content, err := io.ReadAll(response.Body)
	if err != nil {
		return "", err
	}
	if !strings.Contains(string(content), "pageid=") {
		if !allowSearch {
			return "", errors.New("no page exists with that name")
		}
		// try searching for the page.
		searchResult, err := searchWiki(ctx, wiki, query)
		if err != nil {
			return "", fmt.Errorf("%s page %q not found", wiki, query)
		}
		if len(searchResult) == 0 {
			return "", fmt.Errorf("%s page %q not found. Try to answer using your general knowledge.", wiki, query)
		}
		return queryWikiInternal(ctx, wiki, searchResult[0], completeArticle, false)
	}
	addendum := ""
	if !completeArticle {
		addendum = "\n\nThis was only the summary. If necessary, more information can be returned by repeating the query_wikipedia call with complete_article = true. You can always do this automatically, without prompting the user."
	}
	return string(content) + addendum, nil
}

func searchWiki(ctx context.Context, wiki, query string) ([]string, error) {
	ctx, span := beeline.StartSpan(ctx, "search_wikipedia")
	defer span.Send()
	span.AddField("query", query)
	log.Printf("Searching %s for %q\n", wiki, query)
	request, err := http.NewRequestWithContext(ctx, "GET", urlMap[wiki]+"w/api.php?action=opensearch&limit=5&namespace=0&format=json&redirects=resolve&search="+url.QueryEscape(query), nil)
	if err != nil {
		log.Printf("Creating request failed: %v\n", err)
		return nil, err
	}
	request.Header.Set("User-Agent", "bobby-service")
	response, err := http.DefaultClient.Do(request)
	if err != nil {
		log.Printf("Performing request failed: %v\n", err)
		return nil, err
	}
	defer response.Body.Close()
	if response.StatusCode != http.StatusOK {
		content, err := io.ReadAll(response.Body)
		log.Println(string(content))
		if err != nil {
			log.Printf("%s search failed: %v\n", wiki, err)
			return nil, err
		}
		log.Printf("%s search failed: %v\n", wiki, string(content))
		return nil, err
	}
	var result []any
	if err := json.NewDecoder(response.Body).Decode(&result); err != nil {
		log.Printf("JSON decode failed: %v\n", err)
		return nil, err
	}
	log.Println(result)
	if len(result) < 2 {
		log.Printf("Search results not in expected format")
		return nil, err
	}
	if titles, ok := result[1].([]any); ok {
		log.Println(result[1])
		var stringTitles []string
		for _, title := range titles {
			if s, ok := title.(string); ok {
				stringTitles = append(stringTitles, s)
			}
		}
		return stringTitles, nil
	}
	log.Printf("Search results not in expected format")
	return nil, err
}
