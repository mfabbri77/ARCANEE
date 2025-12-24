; Keywords
[
  "break"
  "case"
  "catch"
  "class"
  "clone"
  "const"
  "continue"
  "default"
  "delete"
  "do"
  "else"
  "enum"
  "extends"
  "for"
  "foreach"
  "function"
  "if"
  "in"
  "instanceof"
  "local"
  "null"
  "resume"
  "return"
  "switch"
  "this"
  "throw"
  "try"
  "typeof"
  "while"
  "yield"
  "constructor"
] @keyword

; Literals
(string_literal) @string
(integer_literal) @number
(float_literal) @number
(boolean_literal) @keyword
(null_literal) @keyword

; Comments
(comment) @comment
(block_comment) @comment

; Functions
(function_declaration
  name: (identifier) @function)

(class_declaration
  name: (identifier) @type)
