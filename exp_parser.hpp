/*
    Copyright 2005-2007 Adobe Systems Incorporated
    Distributed under the MIT License (see accompanying file LICENSE_1_0_0.txt
    or a copy at http://stlab.adobe.com/licenses.html)
*/

/*************************************************************************************************/

#ifndef EOP_EXPRESSION_PARSER_HPP
#define EOP_EXPRESSION_PARSER_HPP

#include <adobe/config.hpp>

#include <adobe/array_fwd.hpp>
#include <adobe/istream.hpp>
#include <adobe/dictionary_fwd.hpp>

#include <boost/noncopyable.hpp>

#include "eop_lex_stream_fwd.hpp"

/*************************************************************************************************/

namespace eop {

using namespace adobe;

/*************************************************************************************************/

/*! \page expression_language Expression Language

\code

translation_unit            = { declaration } eof.

%----------------------------------------------------------------

%----------------------------------------------------------------

template      = template_decl declaration.
template_decl = "template" "<" [parameter_list] ">" [constraint].
constraint    = "requires" "(" expression ")".

declaration   = template | structure | enumeration | function.


%----------------------------------------------------------------
structure        = "struct" structure_decl [structure_body] ";".
structure_decl   = identifier | template_name.
structure_name   = identifier.
structure_body   = "{" {member} "}".
member           = constructor | destructor | typed_member
                     | typedef.
typed_member     = expression (
                     (identifier ["[" expression "]"] ";")
                     | member_operator).
constructor      = structure_name "(" [parameter_list] ")"
                     [":" initializer_list] compound.
destructor       = "~" structure_name "(" ")" compound.
member_operator  = "operator" (assignment_body | index | apply).
assignment_body  = "=" "(" parameter ")" compound.
index            = "[" "]" "(" parameter ")" compound.
apply            = "(" ")" "(" [ parameter_list ] ")" compound.
initializer_list = initializer {"," initializer}.
initializer      = identifer "(" [expression_list] ")".

%----------------------------------------------------------------
enumeration    = "enum" identifier "{" identifer_list "}" ";".
identifer_list = identifier {"," identifier}.

%----------------------------------------------------------------
function       = expression function_name
                   "(" [parameter_list] ")" (compound | ";").
function_name  = identifier | structure_name | operator.
operator       = "operator" ("==" | "<" | "+" | "-" | "*" | "/"
                   | "%").
parameter_list = parameter {"," parameter}.
parameter      = expression [identifier].
%----------------------------------------------------------------

statement         = [identifier ":"] (simple_statement
                      | control_statement | typedef).
simple_statement  = expression [assignment | construction] ";".
assignment        = "=" expression.
construction      = identifier [("(" expression_list ")")
                      | assignment | ("[" expression "]")].
control_statement = return | conditional | while | do 
                      | compound | switch | goto.
return            = "return" [expression] ";".
typedef           = "typedef" expression identifier ";".
conditional       = "if" "(" expression ")" statement 
                      ["else" statement].
while             = "while" "(" expression ")" statement.
do                = "do" statement 
                      "while" "(" expression ")" ";".
compound          = "{" {statement} "}".
switch            = "switch" "(" expression ")" "{" {case} "}".
case              = "case" expression ":" {statement}.
goto              = "goto" identifier ";"

%----------------------------------------------------------------

expression      = and {"||" and}.
and             = equality {"&&" equality}.
equality        = relational {("==" | "!=") relational}.
relational      = additive {("<" | ">" | "<=" | ">=") additive}.
additive        = multiplicative {("+" | "-") multiplicative}.
multiplicative  = unary {("*" | "/" | "%") unary}.
unary           = postfix 
                    | ( ("+" | "-" | "!" | "*" | "const") unary).
postfix         = primary {("[" expression "]") 
                    | ("." identifier) 
                    | ("(" [expression_list] ")") | "&"}.
primary         = number | "true" | "false" | string
                    | identifier | "typename" | template_name
                    | ("(" expression ")").
template_name   = structure_name ["<" additive_list ">"].
additive_list   = additive {"," additive}.
expression_list = expression {"," expression}.

Intrinsic Functions: 
    pointer(T) -> T*
    bit_and
    bit_or
    bit_complement
    bit_shift_left
    bit_shift_right
    ? reference(T) ->T&

Notes:
    class_constructor
        class_name must match current class name.


    expression_template
        uses expression_shift_list to avoid ambiguity with >. Use parenthesis for more complex 
        expressions. For example T<(a == b)>.

        if class_name names a template then the optional portion is optional, otherwise
        it is ommited.

    function_name
        points out need to change the name of class_name - perhaps <template_name>
        
    class_friend can go away?
    
class_friend                = "friend" function_declaration.

    

\endcode

*/

/*************************************************************************************************/

class expression_parser : public boost::noncopyable
{
 public:
        
    expression_parser(std::istream& in, const line_position_t& position);
        
    ~expression_parser();
    
    const line_position_t& next_position();


//  translation_unit            = { declaration } eof.
    void parse();

//  template_declaration        = template_declarator declaration.
    bool is_template_declaration();
//  template_declarator         = "template" "<" [ function_parameter_list ] ">" [ template_constraint ].
    bool is_template_declarator();
//  template_constraint         = "requires" "(" expression ")".
    bool is_template_constraint();

//  declaration                 = function_declaration | class_declaration | enumeration
//                                  | template_declaration.
    bool is_declaration(bool);

//  class_declaration           = "struct" class_declarator [ class_body ] ";".
    bool is_class_declaration(bool in_template, bool in_class = false);
//  class_declarator            = identifier | expession_template.
    bool is_class_declarator(name_t&);
//  class_name                  = identifier.
    bool is_class_name(name_t&, bool&);
//  class_body                  = "{" { class_member } "}".
    bool is_class_body(name_t);
//  class_member                = class_constructor | class_destructor | class_typed_member
//                                  | statement_typedef.
    bool is_class_member(name_t);
//  class_typed_member          = expression ((identifier [ "[" expression "]" ] ";") | class_operator).
    bool is_class_typed_member();
//  class_constructor           = class_name "(" [ function_parameter_list ] ")"
//                                  [ ":" class_initializer_list ] statement_compound.
    bool is_class_constructor(name_t);
//  class_destructor            = "~" class_name "(" ")" statement_compound.
    bool is_class_destructor(name_t);
//  class_operator              = "operator" ( class_assignment | class_index | class_apply ).
    bool is_class_operator();
//  class_assignment            = "=" "(" function_parameter ")" statement_compound.
    bool is_class_assignment();
//  class_index                 = "[" "]" "(" function_parameter ")" statement_compound.
    bool is_class_index();
//  class_apply                 = "(" ")" "(" [ function_parameter_list ] ")" statement_compound.
    bool is_class_apply();
//  class_initializer_list      = class_initializer { "," class_initializer }.
    bool is_class_initializer_list();
//  class_initializer           = identifer "(" [expression_list] ")".
    bool is_class_initializer();
#if 0
//  class_friend                = "friend" function_declaration.
    bool is_class_friend();
#endif
#if 0
//  class_member_template       = template_declarator class_member.
    bool is_class_member_template(name_t);
#endif

//  enumeration            = "enum" identifier "{" identifier { "," identifier } "}" ";"
    bool is_enum_declaration();

//  function_declaration        = expression function_name "(" [ function_parameter_list ] ")"
//                                  (statement_compound | ";").
    bool is_function_declaration(bool in_template);
//  function_name               = identifier | class_name | function_operator.
    bool is_function_name(name_t&);
//  function_operator           = "operator" ("==" | "<" | "+" | "-" | "*" | "/" | "%").
    bool is_function_operator();
//  function_parameter_list     = function_parameter { "," function_parameter }.
    bool is_function_parameter_list();
//  function_parameter          = expression [ identifier ].
    bool is_function_parameter();

//  statement                   = [identifier ":"] statement_expression | statement_return
//                                  | statement_typedef | statement_conditional | statement_while
//                                  | statement_do | statement_compound | statement_switch
//                                  | statement_goto.
    bool is_statement();
//  statement_expression        = expression [ statement_assignment |  statement_constructor] ";".
    bool is_statement_expression();
//  statement_assignment        = "=" expression.
    bool is_statement_assignment();
//  statement_constructor       = identifier [ ("(" expression_list ")") | statement_assignment
//                                              | ("[" expression "]") ].
    bool is_statement_constructor();
//  statement_return            = "return" [ expression ] ";".
    bool is_statement_return();
//  statement_typedef           = "typedef" expression identifier ";".
    bool is_statement_typedef();
//  statement_conditional       = "if" "(" expression ")" statement [ "else" statement ].
    bool is_statement_conditional();
//  statement_while             = "while" "(" expression ")" statement.
    bool is_statement_while();
//  statement_do                = "do" statement "while" "(" expression ")" ";".
    bool is_statement_do();
//  statement_compound          = "{" { statement } "}".
    bool is_statement_compound();
//  statement_switch            = "switch" "(" expression ")" "{" { statement_case } "}".
    bool is_statement_switch();
//  statement_case              = "case" expression ":" { statement }.
    bool is_statement_case();
//  statement_goto              = "goto" identifier ";"
    bool is_statement_goto();

//  expression                  = expression_and { "||" expression_and }.
    bool is_expression(array_t&);
    void require_expression(array_t&);
    
//  expression_and              = expression_equality { "&&" expression_equality }.
    bool is_expression_and(array_t&);
//  expression_bit_and          = expression_equality { "bitand" expression_equality }.
    bool is_expression_bit_and(array_t&);
//  expression_equality         = expression_relational { ("==" | "!=") expression_relational }.
    bool is_expression_equality(array_t&);
//  expression_relational       = expression_additive { ("<" | ">" | "<=" | ">=") expression_additive }.
    bool is_expression_relational(array_t&);
//  expression_additive = expression_multiplicative { ("+" | "-") expression_multiplicative }.
    bool is_expression_additive(array_t&);
    bool is_additive_operator(name_t&);
//  expression_multiplicative = expression_unary { ("*" | "/" | "%") expression_unary }.
    bool is_expression_multiplicative(array_t&);
    bool is_multiplicative_operator(name_t&);
//  expression_unary = expression_postfix | ( ("+" | "-" | "!" | "*" | "&" | "const") expression_unary).
    bool is_expression_unary(array_t&);
    bool is_unary_operator(name_t&); // helper
//  expression_postfix          = expression_primary { ("[" expression "]") | ("." identifier) | ("(" [expression_list] ")") | "&" }.
    bool is_expression_postfix(array_t&);
//  expression_primary          = number | "true" | "false" | string | identifier | "typename" | expression_template | ("(" expression ")").
    bool is_expression_primary(array_t&);
//  expression_template         = class_name [ "<" expression_list ">" ].
    bool is_expression_template();
//  expression_additive_list    = expression_additive { "," expression_additive }.
    bool is_expression_additive_list();
//  expression_list             = expression { "," expression }.
    bool is_expression_list(array_t&);
    
//  boolean = "true" | "false".
    bool is_boolean(any_regular_t&);
        
//  helper functions
    bool is_relational_operator(name_t&);
    bool is_operator_shift(name_t&);
    
//  lexical tokens:

    bool is_identifier(name_t&);
    bool is_identifier();
    void require_identifier();
    bool is_identifier(any_regular_t&);
    void require_identifier(any_regular_t&);


    bool is_lead_comment(std::string&);
    bool is_trail_comment(std::string&);
    
/*
    REVISIT (sparent) : We should provide a protected call to get the token stream and allow
    subclasses to access it directly - but for now we'll stick with the law of Demiter.
*/

 protected:
    const stream_lex_token_t& get_token();
    void putback();

    bool is_token (name_t tokenName, any_regular_t& tokenValue);
    bool is_token (name_t tokenName);
    void require_token (name_t tokenName, any_regular_t& tokenValue);
    void require_token (name_t tokenName);
    bool is_keyword (name_t keywordName);
    void require_keyword (name_t keywordName);

    void throw_exception (const char* errorString);
    void throw_exception (const name_t& found, const name_t& expected);

private:
    void set_keyword_extension_lookup(const keyword_extension_lookup_proc_t& proc);

    class implementation;
    implementation*     object;
};

/*************************************************************************************************/

} // namespace adobe

/*************************************************************************************************/

#endif

/*************************************************************************************************/
