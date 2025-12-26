; TOML Tree-sitter Highlight Queries [REQ-94]
; Maps TOML syntax elements to capture names for theme-driven highlighting

; Comments
(comment) @comment

; Strings
(string) @string
(escape_sequence) @string.special

; Boolean
(boolean) @constant.builtin

; Numbers
(integer) @number
(float) @number.float
(local_date) @number
(local_time) @number
(local_date_time) @number
(offset_date_time) @number

; Keys
(bare_key) @property
(quoted_key) @property
(dotted_key) @property

; Tables
(table (bare_key) @type)
(table (quoted_key) @type)
(table (dotted_key) @type)

; Array of tables
(table_array_element (bare_key) @type)
(table_array_element (quoted_key) @type)
(table_array_element (dotted_key) @type)

; Punctuation
"[" @punctuation.bracket
"]" @punctuation.bracket
"[[" @punctuation.bracket
"]]" @punctuation.bracket
"{" @punctuation.bracket
"}" @punctuation.bracket
"=" @operator
"." @punctuation.delimiter
"," @punctuation.delimiter
