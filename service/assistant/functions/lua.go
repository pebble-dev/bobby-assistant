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
	"fmt"
	"log"
	"math"
	"strings"
	"time"

	"github.com/pebble-dev/bobby-assistant/service/assistant/query"
	"github.com/yuin/gopher-lua"
	"google.golang.org/genai"
)

type LuaInput struct {
	// The Lua 5.2 script to run. Remember the return statements!
	Script string `json:"script" jsonschema:"required"`
	// If necessary, the timezone name to assume when running the script. Default's to the user's local time.
	Timezone string `json:"timezone"`
}

func init() {
	registerFunction(Registration{
		Definition: genai.FunctionDeclaration{
			Name:        "lua",
			Description: "Runs the provided Lua 5.2 script, and returns the result. ONLY STANDARD LIBRARY FUNCTIONS ARE AVAILABLE. DO NOT CALL ANYTHING ELSE.",
			Parameters: &genai.Schema{
				Type:     genai.TypeObject,
				Nullable: false,
				Properties: map[string]*genai.Schema{
					"script": {
						Type:        genai.TypeString,
						Description: "The Lua 5.2 script to run. Remember the return statements!",
						Nullable:    false,
					},
					"timezone": {
						Type:        genai.TypeString,
						Description: "If necessary, the timezone name to assume when running the script. Defaults to the user's local time.",
						Nullable:    true,
					},
				},
				Required: []string{"script"},
			},
		},
		Fn:        luaImplementation,
		Thought:   luaThought,
		InputType: LuaInput{},
	})
}

func luaThought(args interface{}) string {
	return "Getting a calculator"
}

func luaImplementation(ctx context.Context, args interface{}) interface{} {
	arg := args.(*LuaInput)
	result, err := runLua(ctx, arg.Timezone, arg.Script)
	if err != nil || result == nil {
		if e, ok := err.(*lua.ApiError); (result == nil && err == nil) || (ok && e.Type == lua.ApiErrorSyntax) {
			log.Println(err)
			lines := strings.Split(arg.Script, "\n")
			lines[len(lines)-1] = "return " + lines[len(lines)-1]
			arg.Script = strings.Join(lines, "\n")
			result, err = runLua(ctx, arg.Timezone, arg.Script)
			if err != nil {
				return Error{"Script execution failed: " + err.Error()}
			}
		} else {
			return Error{"Script execution failed: " + err.Error()}
		}
	}
	return map[string]any{"result": convertValueToJsonCompatible(result)}
}

func convertValueToJsonCompatible(v interface{}) interface{} {
	switch v := v.(type) {
	case map[interface{}]interface{}:
		ret := make(map[string]interface{})
		for k, v := range v {
			ret[fmt.Sprint(k)] = convertValueToJsonCompatible(v)
		}
		return ret
	case []interface{}:
		ret := make([]interface{}, len(v))
		for i, v := range v {
			ret[i] = convertValueToJsonCompatible(v)
		}
		return ret
	default:
		return v
	}
}

func runLua(ctx context.Context, timezone, script string) (interface{}, error) {
	log.Println("Running script:", script)
	ctx, cancelFunc := context.WithTimeout(ctx, 100*time.Millisecond)
	defer cancelFunc()
	l := lua.NewState(lua.Options{
		SkipOpenLibs: true,
	})
	l.SetContext(ctx)
	defer l.Close()
	openSafeLibraries(l)
	l.SetField(l.GetGlobal("os"), "date", l.NewFunction(func(L *lua.LState) int {
		return osDate(L, timezone, query.TzOffsetFromContext(ctx))
	}))
	l.SetField(l.GetGlobal("os"), "time", l.NewFunction(func(L *lua.LState) int {
		return osTime(L, timezone, query.TzOffsetFromContext(ctx))
	}))
	if err := l.DoString(script); err != nil {
		return nil, err
	}
	if l.GetTop() < 2 {
		ret := toGoValue(l.Get(-1))
		if r, ok := ret.(float64); ok {
			if r > 1000000000 && r < 3000000000 && strings.Contains(script, "os.time") {
				location := time.UTC
				if timezone != "" {
					if loc, err := time.LoadLocation(timezone); err == nil {
						location = loc
					}
				} else {
					tzOffset := query.TzOffsetFromContext(ctx)
					tzName := fmt.Sprintf("UTC+%+d", tzOffset/60)
					if tzOffset%60 != 0 {
						tzName = fmt.Sprintf("%s:%02d", tzName, tzOffset%60)
					}
					location = time.FixedZone(tzName, tzOffset*60)
				}
				return map[string]interface{}{
					"result":     ret,
					"asDateTime": time.Unix(int64(r), 0).In(location).Format(time.RFC1123),
				}, nil
			}
		}
		return ret, nil
	}
	var rets []interface{}
	for i := 1; i <= l.GetTop(); i++ {
		rets = append(rets, toGoValue(l.Get(i-l.GetTop()-1)))
	}
	return rets, nil
}

func evalExpression(expression string) string {
	ctx, cancel := context.WithTimeout(context.Background(), 100*time.Millisecond)
	defer cancel()
	l := lua.NewState(lua.Options{
		SkipOpenLibs: true,
	})
	defer l.Close()
	l.SetContext(ctx)
	if err := l.DoString("return " + expression); err != nil {
		log.Printf("Couldn't evaluate %q using lua. Error: %v", expression, err)
		return expression
	}
	return fmt.Sprint(toGoValue(l.Get(-1)))
}

func toGoValue(lv lua.LValue) interface{} {
	switch v := lv.(type) {
	case *lua.LNilType:
		return nil
	case lua.LBool:
		return bool(v)
	case lua.LString:
		return string(v)
	case lua.LNumber:
		f := float64(v)
		if math.Abs(f) < 1e-14 {
			return 0
		}
		return f
	case *lua.LTable:
		maxn := v.MaxN()
		if maxn == 0 { // table
			ret := make(map[interface{}]interface{})
			v.ForEach(func(key, value lua.LValue) {
				keystr := fmt.Sprint(toGoValue(key))
				ret[keystr] = toGoValue(value)
			})
			return ret
		} else { // array
			ret := make([]interface{}, 0, maxn)
			for i := 1; i <= maxn; i++ {
				ret = append(ret, toGoValue(v.RawGetInt(i)))
			}
			return ret
		}
	default:
		return v
	}
}

func openSafeLibraries(l *lua.LState) {
	for _, pair := range []struct {
		n string
		f lua.LGFunction
	}{
		//{lua.LoadLibName, lua.OpenPackage}, // Must be first
		{lua.BaseLibName, lua.OpenBase},
		{lua.TabLibName, lua.OpenTable},
		{lua.StringLibName, lua.OpenString},
		{lua.MathLibName, lua.OpenMath},
		{lua.OsLibName, lua.OpenOs},
	} {
		if err := l.CallByParam(lua.P{
			Fn:      l.NewFunction(pair.f),
			NRet:    0,
			Protect: true,
		}, lua.LString(pair.n)); err != nil {
			panic(err)
		}
	}
	g := l.GetGlobal("_G")
	l.SetField(g, "collectgarbage", lua.LNil)
	l.SetField(g, "dofile", lua.LNil)
	l.SetField(g, "loadfile", lua.LNil)
	l.SetField(g, "print", lua.LNil)
	l.SetField(g, "_printregs", lua.LNil)
	l.SetField(g, "module", lua.LNil)
	l.SetField(g, "require", lua.LNil)
	l.SetField(g, "newproxy", lua.LNil)
	os := l.GetGlobal("os")
	l.SetField(os, "execute", lua.LNil)
	l.SetField(os, "exit", lua.LNil)
	l.SetField(os, "getenv", lua.LNil)
	l.SetField(os, "remove", lua.LNil)
	l.SetField(os, "rename", lua.LNil)
	l.SetField(os, "setenv", lua.LNil)
	l.SetField(os, "setlocale", lua.LNil)
	l.SetField(os, "tmpname", lua.LNil)
	str := l.GetGlobal("string")
	l.SetField(str, "rep", l.NewFunction(strRep))
}

func strRep(L *lua.LState) int {
	str := L.CheckString(1)
	n := L.CheckInt(2)
	if n < 0 {
		L.Push(lua.LString(""))
	} else {
		if len(str)*n > 200 {
			L.RaiseError("Maximum string length exceeded")
			return 0
		}
		L.Push(lua.LString(strings.Repeat(str, n)))
	}
	return 1
}

func osTime(L *lua.LState, timezone string, userTzOffset int) int {
	if L.GetTop() == 0 {
		// If there are no arguments, timezone is not a relevant consideration, so just return the current time.
		L.Push(lua.LNumber(time.Now().Unix()))
		return 1
	}
	// If a table is present, then we are supposed to construct a timezone in the local timezone (which we have promised
	// is either the user's local timezone, or whatever timezone the model felt like passing us).
	table := L.CheckTable(1)
	year, ok := table.RawGetString("year").(lua.LNumber)
	if !ok {
		L.RaiseError("year is required and must be a number")
		return 0
	}
	month, ok := table.RawGetString("month").(lua.LNumber)
	if !ok {
		L.RaiseError("month is required and must be a number")
		return 0
	}
	day, ok := table.RawGetString("day").(lua.LNumber)
	if !ok {
		L.RaiseError("day is required and must be a number")
		return 0
	}
	hour, ok := table.RawGetString("hour").(lua.LNumber)
	if !ok {
		hour = 12
	}
	minute, ok := table.RawGetString("min").(lua.LNumber)
	if !ok {
		minute = 0
	}
	second, ok := table.RawGetString("sec").(lua.LNumber)
	if !ok {
		second = 0
	}
	location := time.UTC
	if timezone != "" {
		l, err := time.LoadLocation(timezone)
		if err != nil {
			L.RaiseError("Invalid timezone %q: %v", timezone, err)
			return 0
		}
		location = l
	} else {
		tzName := fmt.Sprintf("UTC%+d", userTzOffset/60)
		if userTzOffset%60 != 0 {
			tzName = fmt.Sprintf("%s:%02d", tzName, userTzOffset%60)
		}
		location = time.FixedZone(tzName, userTzOffset*60)
	}
	t := time.Date(int(year), time.Month(month), int(day), int(hour), int(minute), int(second), 0, location)
	L.Push(lua.LNumber(t.Unix()))
	return 1
}

func osDate(L *lua.LState, timezone string, userTzOffset int) int {
	t := time.Now()
	location := time.UTC
	if timezone != "" {
		l, err := time.LoadLocation(timezone)
		if err != nil {
			L.RaiseError("Invalid timezone %q: %v", timezone, err)
			return 0
		}
		location = l
	} else {
		tzName := fmt.Sprintf("UTC%+d", userTzOffset/60)
		if userTzOffset%60 != 0 {
			tzName = fmt.Sprintf("%s:%02d", tzName, userTzOffset%60)
		}
		location = time.FixedZone(tzName, userTzOffset*60)
	}
	t = t.In(location)
	cfmt := "%c"
	if L.GetTop() >= 1 {
		cfmt = L.CheckString(1)
		if strings.HasPrefix(cfmt, "!") {
			t = time.Now().UTC()
			cfmt = strings.TrimLeft(cfmt, "!")
		}
		if L.GetTop() >= 2 {
			t = time.Unix(L.CheckInt64(2), 0).In(location)
		}
		if strings.HasPrefix(cfmt, "*t") {
			ret := L.NewTable()
			ret.RawSetString("year", lua.LNumber(t.Year()))
			ret.RawSetString("month", lua.LNumber(t.Month()))
			ret.RawSetString("day", lua.LNumber(t.Day()))
			ret.RawSetString("hour", lua.LNumber(t.Hour()))
			ret.RawSetString("min", lua.LNumber(t.Minute()))
			ret.RawSetString("sec", lua.LNumber(t.Second()))
			ret.RawSetString("wday", lua.LNumber(t.Weekday()+1))
			// TODO yday & dst
			ret.RawSetString("yday", lua.LNumber(0))
			ret.RawSetString("isdst", lua.LFalse)
			L.Push(ret)
			return 1
		}
	}
	L.Push(lua.LString(strftime(t, cfmt)))
	return 1
}

var cDateFlagToGo = map[byte]string{
	'a': "mon", 'A': "Monday", 'b': "Jan", 'B': "January", 'c': "02 Jan 06 15:04 MST", 'd': "02",
	'F': "2006-01-02", 'H': "15", 'I': "03", 'm': "01", 'M': "04", 'p': "PM", 'P': "pm", 'S': "05",
	'x': "15/04/05", 'X': "15:04:05", 'y': "06", 'Y': "2006", 'z': "-0700", 'Z': "MST"}

func strftime(t time.Time, cfmt string) string {
	sc := newFlagScanner('%', "", "", cfmt)
	for c, eos := sc.Next(); !eos; c, eos = sc.Next() {
		if !sc.ChangeFlag {
			if sc.HasFlag {
				if v, ok := cDateFlagToGo[c]; ok {
					sc.AppendString(t.Format(v))
				} else {
					switch c {
					case 'w':
						sc.AppendString(fmt.Sprint(int(t.Weekday())))
					default:
						sc.AppendChar('%')
						sc.AppendChar(c)
					}
				}
				sc.HasFlag = false
			} else {
				sc.AppendChar(c)
			}
		}
	}

	return sc.String()
}

type flagScanner struct {
	flag       byte
	start      string
	end        string
	buf        []byte
	str        string
	Length     int
	Pos        int
	HasFlag    bool
	ChangeFlag bool
}

func newFlagScanner(flag byte, start, end, str string) *flagScanner {
	return &flagScanner{flag, start, end, make([]byte, 0, len(str)), str, len(str), 0, false, false}
}

func (fs *flagScanner) AppendString(str string) { fs.buf = append(fs.buf, str...) }

func (fs *flagScanner) AppendChar(ch byte) { fs.buf = append(fs.buf, ch) }

func (fs *flagScanner) String() string { return string(fs.buf) }

func (fs *flagScanner) Next() (byte, bool) {
	c := byte('\000')
	fs.ChangeFlag = false
	if fs.Pos == fs.Length {
		if fs.HasFlag {
			fs.AppendString(fs.end)
		}
		return c, true
	} else {
		c = fs.str[fs.Pos]
		if c == fs.flag {
			if fs.Pos < (fs.Length-1) && fs.str[fs.Pos+1] == fs.flag {
				fs.HasFlag = false
				fs.AppendChar(fs.flag)
				fs.Pos += 2
				return fs.Next()
			} else if fs.Pos != fs.Length-1 {
				if fs.HasFlag {
					fs.AppendString(fs.end)
				}
				fs.AppendString(fs.start)
				fs.ChangeFlag = true
				fs.HasFlag = true
			}
		}
	}
	fs.Pos++
	return c, false
}
