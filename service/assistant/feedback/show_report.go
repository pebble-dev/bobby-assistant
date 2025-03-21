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

package feedback

import (
	_ "embed"
	"github.com/pebble-dev/bobby-assistant/service/assistant/util/storage"
	"html/template"
	"log"
	"net/http"
	"path"
)

//go:embed report.html
var reportTemplateString string
var reportTemplate = template.Must(template.New("report").Parse(reportTemplateString))

func HandleShowReport(rw http.ResponseWriter, r *http.Request) {
	ctx := r.Context()
	rd := storage.GetRedis()

	_, reportId := path.Split(r.URL.Path)

	reportedThread, err := loadReport(ctx, rd, reportId)
	if err != nil {
		log.Printf("Error getting reported thread: %v", err)
		http.Error(rw, err.Error(), http.StatusInternalServerError)
		return
	}

	reportTemplate.ExecuteTemplate(rw, "report", reportedThread)
}
