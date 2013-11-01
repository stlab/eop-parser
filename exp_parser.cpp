/*
    Copyright 2005-2007 Adobe Systems Incorporated
    Distributed under the MIT License (see accompanying file LICENSE_1_0_0.txt
    or a copy at http://stlab.adobe.com/licenses.html)
*/

/*************************************************************************************************/

/*
REVISIT (sparent) : [Old comment - what does this mean?] I need to handle the pos_type correctly
with regards to state.
*/

/*************************************************************************************************/

#include <utility>
#include <istream>
#include <sstream>
#include <iomanip>
#include <cassert>

#include <boost/config.hpp>

#include <adobe/array.hpp>
#include <adobe/name.hpp>
#include <adobe/any_regular.hpp>
#include <adobe/dictionary.hpp>

#include <adobe/implementation/token.hpp>

#include <adobe/string.hpp>

#include <adobe/implementation/parser_shared.hpp>

#include "exp_parser.hpp"
#include "eop_lex_stream.hpp"

#ifdef BOOST_MSVC
namespace std {
    using ::isspace;
    using ::isalnum;
    using ::isalpha;
    using ::isdigit;
}
#endif

/*************************************************************************************************/

namespace eop {

/*************************************************************************************************/

class expression_parser::implementation
{
 public:
    implementation(std::istream& in, const line_position_t& position) :
        token_stream_m(in, position)
        { }

    void set_keyword_extension_lookup(const keyword_extension_lookup_proc_t& proc)
        { token_stream_m.set_keyword_extension_lookup(proc); }

    lex_stream_t token_stream_m;

    typedef closed_hash_map<name_t, bool> class_name_index_t;
    class_name_index_t class_name_index_m;
};

aggregate_name_t template_k     = { "template" };
aggregate_name_t typename_k     = { "typename" };
aggregate_name_t requires_k     = { "requires" };
aggregate_name_t const_k        = { "const" };
aggregate_name_t return_k       = { "return" };
aggregate_name_t struct_k       = { "struct" };
aggregate_name_t operator_k     = { "operator" };
aggregate_name_t typedef_k      = { "typedef" };
aggregate_name_t if_k           = { "if" };
aggregate_name_t else_k         = { "else" };
aggregate_name_t while_k        = { "while" };
aggregate_name_t bitand_k       = { "bitand" };
aggregate_name_t friend_k       = { "friend" };
aggregate_name_t do_k           = { "do" };
aggregate_name_t enum_k         = { "enum" };
aggregate_name_t case_k         = { "case" };
aggregate_name_t switch_k       = { "switch" };
aggregate_name_t goto_k         = { "goto" };


bool keyword_lookup(const name_t& x)
{
    if (x == template_k) return true;
    if (x == typename_k) return true;
    if (x == requires_k) return true;
    if (x == const_k) return true;
    if (x == return_k) return true;
    if (x == struct_k) return true;
    if (x == operator_k) return true;
    if (x == typedef_k) return true;
    if (x == if_k) return true;
    if (x == else_k) return true;
    if (x == while_k) return true;
    if (x == bitand_k) return true;
    if (x == friend_k) return true;
    if (x == do_k) return true;
    if (x == enum_k) return true;
    if (x == case_k) return true;
    if (x == switch_k) return true;
    if (x == goto_k) return true;
    return false;
}

expression_parser::expression_parser(std::istream& in, const line_position_t& position) :
        object(new implementation(in, position))
{
    set_keyword_extension_lookup(&keyword_lookup);
}

expression_parser::~expression_parser()
    { delete object; }

/*************************************************************************************************/

/* REVISIT (sparent) : Should this be const? And is there a way to specify the class to throw? */

void expression_parser::throw_exception(const char* errorString)
{
    std::string error = errorString;

    error << " Found \"" << get_token().first.c_str() << "\".";
    putback();

    throw adobe::stream_error_t(error, next_position());
}

/*************************************************************************************************/

/* REVISIT (sparent) : Should this be const? And is there a way to specify the class to throw? */

void expression_parser::throw_exception(const name_t& found, const name_t& expected)
    { throw_parser_exception(found, expected, next_position()); }

/*************************************************************************************************/

const line_position_t& expression_parser::next_position()
{
    return object->token_stream_m.next_position();
}

/*************************************************************************************************/

void expression_parser::set_keyword_extension_lookup(const keyword_extension_lookup_proc_t& proc)
{
    object->set_keyword_extension_lookup(proc);
}

/*************************************************************************************************/

//  translation_unit            = { declaration } eof.
void expression_parser::parse()
{
    name_t name;

    while (is_declaration(name)) ;
    require_token(eof_k);
}

/*************************************************************************************************/

//  declaration                 = function_declaration | class_declaration | enum_declaration
//                                  | template_declaration.
bool expression_parser::is_declaration(bool in_template)
{
    return is_function_declaration(in_template) || is_class_declaration(in_template)
            || is_enum_declaration() || is_template_declaration();
}

/*************************************************************************************************/
//  template_declaration        = template_declarator declaration.
bool expression_parser::is_template_declaration()
{
    if (!is_template_declarator()) return false;
    if (!is_declaration(true)) throw_exception("declaration required.");
    return true;
}

/*************************************************************************************************/
//  template_declarator         = "template" "<" [ function_parameter_list ] ">" [ template_constraint ].
bool expression_parser::is_template_declarator()
{
    if (!is_keyword(template_k)) return false;
    require_token(less_k);
    is_function_parameter_list();
    require_token(greater_k);
    is_template_constraint();
    return true;
}

/*************************************************************************************************/

//  constraint                  = "requires" "(" expression ")".
bool expression_parser::is_template_constraint()
{
    array_t tmp;

    if (!is_keyword(requires_k)) return false;
    require_token(open_parenthesis_k);
    if (!is_expression(tmp)) throw_exception("expression required.");
    require_token(close_parenthesis_k);
    return true;
}

/*************************************************************************************************/
//  class_declaration           = "struct" class_declarator [ class_body ] ";".
bool expression_parser::is_class_declaration(bool in_template, bool in_class)
{
    name_t name;

    if (!is_keyword(struct_k)) return false;
    if (!is_class_declarator(name)) throw_exception("class_name required.");
    if (name && !in_class) object->class_name_index_m.insert(make_pair(name, in_template));
    is_class_body(name);
    require_token(semicolon_k);
    return true;
}

/*************************************************************************************************/
//  class_declarator            = identifier | expression_template.
bool expression_parser::is_class_declarator(name_t& name)
{
    return is_identifier(name) || is_expression_template();
}

/*************************************************************************************************/
//  class_name                  = identifier.
bool expression_parser::is_class_name(name_t& class_name, bool& is_template)
{
    any_regular_t x;
    if (!is_token(identifier_k, x)) return false;

    implementation::class_name_index_t::const_iterator f = object->class_name_index_m.find(x.cast<name_t>());
    if (f == object->class_name_index_m.end()) { putback(); return false; }
    class_name = f->first;
    is_template = f->second;
    return true;
}

/*************************************************************************************************/
//  class_body                  = "{" { class_member } "}".
bool expression_parser::is_class_body(name_t this_class)
{
    if (!is_token(open_brace_k)) return false;
    while (is_class_member(this_class)) ;
    require_token(close_brace_k);
    return true;
}

/*************************************************************************************************/
//  class_member                = class_constructor | class_destructor | class_typed_member
//                                  | statement_typedef.
bool expression_parser::is_class_member(name_t this_class)
{
    return is_class_constructor(this_class) || is_class_destructor(this_class)
        || is_class_typed_member() || is_statement_typedef();
        /* || is_class_member_template(this_class) || is_class_declaration(false, true); */
}

/*************************************************************************************************/
//  enum_declaration            = "enum" identifier "{" identifier { "," identifier } "}" ";"
bool expression_parser::is_enum_declaration()
{
    if (!is_keyword(enum_k)) return false;
    require_identifier();
    require_token(open_brace_k);
    require_identifier();
    while (is_token(comma_k)) require_identifier();
    require_token(close_brace_k);
    require_token(semicolon_k);
    return true;
}

/*************************************************************************************************/
//  class_typed_member          = expression ((identifier [ "[" expression "]" ] ";") | class_operator).
bool expression_parser::is_class_typed_member()
{
    array_t tmp;

    if (!is_expression(tmp)) return false;
    if (is_identifier()) {
        if (is_token(open_bracket_k)) {
            require_expression(tmp);
            require_token(close_bracket_k);
        }
        require_token(semicolon_k);
        return true;
    }
    if (!is_class_operator()) throw_exception("identifier or class_operator required.");\
    return true;
}

/*************************************************************************************************/
//  class_constructor           = class_name "(" [ function_parameter_list ] ")"
//                                  [ ":" class_initializer_list ] statement_compound.
bool expression_parser::is_class_constructor(name_t this_class)
{
    bool tmp;
    name_t class_name;

    if (!is_class_name(class_name, tmp)) return false;
    if (class_name != this_class) { putback(); return false; }
    require_token(open_parenthesis_k);
    is_function_parameter_list();
    require_token(close_parenthesis_k);
    if (is_token(colon_k)) {
        if (!is_class_initializer_list()) throw_exception("class_initializer_list required.");
    }
    if (!is_statement_compound()) throw_exception("statement_compound required.");
    return true;
}

/*************************************************************************************************/
//  class_destructor            = "~" class_name "(" ")" statement_compound.
bool expression_parser::is_class_destructor(name_t this_class)
{
    bool tmp;
    name_t class_name;

    if (!is_token(destructor_k)) return false;
    if (!is_class_name(class_name, tmp)) throw_exception("class_name required.");
    if (class_name != this_class) { putback(); throw_exception(class_name, this_class); }
    require_token(open_parenthesis_k);
    require_token(close_parenthesis_k);
    if (!is_statement_compound()) throw_exception("statement_compound required.");
    return true;
}

/*************************************************************************************************/
//  class_operator              = "operator" ( class_assignment | class_index | class_apply ).
bool expression_parser::is_class_operator()
{
    if (!is_keyword(operator_k)) return false;
    return is_class_assignment() || is_class_index() || is_class_apply();
}

/*************************************************************************************************/
//  class_assignment            = "=" "(" function_parameter ")" statement_compound.
bool expression_parser::is_class_assignment()
{
    if (!is_token(assign_k)) return false;
    require_token(open_parenthesis_k);
    if (!is_function_parameter()) throw_exception("function_parameter required.");
    require_token(close_parenthesis_k);
    if (!is_statement_compound()) throw_exception("statement_compound required.");
    return true;
}

/*************************************************************************************************/
//  class_index                 = "[" "]" "(" function_parameter ")" statement_compound.
bool expression_parser::is_class_index()
{
    if (!is_token(open_bracket_k)) return false;
    require_token(close_bracket_k);
    require_token(open_parenthesis_k);
    if (!is_function_parameter()) throw_exception("function_parameter required.");
    require_token(close_parenthesis_k);
    if (!is_statement_compound()) throw_exception("statement_compound required.");
    return true;
}

/*************************************************************************************************/
//  class_apply                 = "(" ")" "(" [ function_parameter_list ] ")" statement_compound.
bool expression_parser::is_class_apply()
{
    if (!is_token(open_parenthesis_k)) return false;
    require_token(close_parenthesis_k);
    require_token(open_parenthesis_k);
    is_function_parameter_list();
    require_token(close_parenthesis_k);
    if (!is_statement_compound()) throw_exception("statement_compound required.");
    return true;
}

/*************************************************************************************************/
//  class_initializer_list      = class_initializer { "," class_initializer }.
bool expression_parser::is_class_initializer_list()
{
    if (!is_class_initializer()) return false;
    while (is_token(comma_k)) {
        if (!is_class_initializer()) throw_exception("class_initializer required.");
    }
    return true;
}

/*************************************************************************************************/
//  class_initializer           = identifer "(" [expression_list] ")".
bool expression_parser::is_class_initializer()
{
    array_t tmp;

    if (!is_identifier()) return false;
    require_token(open_parenthesis_k);
    is_expression_list(tmp);
    require_token(close_parenthesis_k);
    return true;
}

/*************************************************************************************************/

#if 0
//  class_friend                = "friend" function_declaration.
bool expression_parser::is_class_friend()
{
    if (!is_keyword(friend_k)) return false;
    if (!is_function_declaration(false)) throw_exception("function_declaration required.");
    return true;
}
#endif

/*************************************************************************************************/

#if 0
//  class_member_template       = template_declarator class_member.
bool expression_parser::is_class_member_template(name_t this_class)
{
    if (!is_template_declarator()) return false;
    if (!is_class_member(this_class)) throw_exception("class_member required.");
    return true;
}
#endif

/*************************************************************************************************/

//  function_declaration        = expression function_name "(" [ function_parameter_list ] ")"
//                                  (statement_compound | ";").
bool expression_parser::is_function_declaration(bool in_template)
{
    array_t tmp;
    name_t name;

    if (!is_expression(tmp)) return false;
    if (!is_function_name(name)) throw_exception("function_name required.");
    if (name) object->class_name_index_m.insert(make_pair(name, in_template));
    require_token(open_parenthesis_k);
    is_function_parameter_list();
    require_token(close_parenthesis_k);
    if (!(is_statement_compound() || is_token(semicolon_k))) {
        throw_exception("statement_compound or semicolon required.");
    }
    return true;
}

/*************************************************************************************************/
//  function_name               = identifier | class_name |function_operator.
bool expression_parser::is_function_name(name_t& name)
{
    name_t tmp;
    bool is_template;

    return is_identifier(name) || is_class_name(tmp, is_template) || is_function_operator();
}

/*************************************************************************************************/
//  function_operator           = "operator" ("==" | "<" | "+" | "-" | "*" | "/" | "%").
bool expression_parser::is_function_operator()
{
    if (!is_keyword(operator_k)) return false;
    if (!(is_token(equal_k) || is_token(less_k) || is_token(add_k) || is_token(subtract_k)
            || is_token(multiply_k) || is_token(divide_k) || is_token(modulus_k))) {
        throw_exception("'==', '<', '+', '-', '*', '/', or '%' required.");
    }
    return true;
}

/*************************************************************************************************/
//  function_parameter_list     = function_parameter { "," function_parameter }.
bool expression_parser::is_function_parameter_list()
{
    if (!is_function_parameter()) return false;
    while (is_token(comma_k)) {
        if (!is_function_parameter()) throw_exception("function_parameter required.");
    }
    return true;
}

/*************************************************************************************************/
//  function_parameter          = expression [ identifier ].
bool expression_parser::is_function_parameter()
{
    array_t tmp;

    if (!is_expression(tmp)) return false;
    is_identifier();
    return true;
}

/*************************************************************************************************/
//  statement                   = [identifier ":"] statement_expression | statement_return
//                                  | statement_typedef | statement_conditional | statement_while
//                                  | statement_do | statement_compound | statement_switch
//                                  | statement_goto.
bool expression_parser::is_statement()
{
    bool has_label = false;

    if (is_identifier()) {
        if (is_token(colon_k)) has_label = true;
        else putback(); // LL(2)
    }

    if (is_statement_expression() || is_statement_return() || is_statement_typedef()
        || is_statement_conditional() || is_statement_while() || is_statement_do()
        || is_statement_compound() || is_statement_switch() || is_statement_goto()) return true;
        
    /*
        REVISIT (sparent) : inablity to name this group is a good reason to split statement from
        labeled statement.
    */
    if (has_label) throw_exception("statement required.");
    
    return false;
}

/*************************************************************************************************/
//  statement_expression        = expression [ statement_assignment |  statement_constructor] ";".
bool expression_parser::is_statement_expression()
{
    array_t tmp;

    if (!is_expression(tmp)) return false;
    is_statement_assignment() || is_statement_constructor();
    require_token(semicolon_k);
    return true;
}

/*************************************************************************************************/
// statement_assignment        = "=" expression.
bool expression_parser::is_statement_assignment()
{
    array_t tmp;

    if (!is_token(assign_k)) return false;
    require_expression(tmp);
    return true;
}

/*************************************************************************************************/
//  statement_constructor       = identifier [ ("(" expression_list ")") | statement_assignment
//                                              | ("[" expression "]") ].
bool expression_parser::is_statement_constructor()
{
    array_t tmp;

    if (!is_identifier()) return false;
    if (is_token(open_parenthesis_k)) {
        if (!is_expression_list(tmp)) throw_exception("expression_list required.");
        require_token(close_parenthesis_k);
    } else if (is_statement_assignment()) {
    } else if (is_token(open_bracket_k)) {
        require_expression(tmp);
        require_token(close_bracket_k);
    }
    return true;
}

/*************************************************************************************************/
//  statement_return            = "return" [ expression ] ";".
bool expression_parser::is_statement_return()
{
    array_t tmp;

    if (!is_keyword(return_k)) return false;
    is_expression(tmp);
    require_token(semicolon_k);
    return true;
}

/*************************************************************************************************/
//  statement_typedef           = "typedef" expression identifier ";".
bool expression_parser::is_statement_typedef()
{
    array_t tmp;

    if (!is_keyword(typedef_k)) return false;
    require_expression(tmp);
    require_identifier();
    require_token(semicolon_k);
    return true;
}

/*************************************************************************************************/
//  statement_conditional       = "if" "(" expression ")" statement [ "else" statement ].
bool expression_parser::is_statement_conditional()
{
    array_t tmp;

    if (!is_keyword(if_k)) return false;
    require_token(open_parenthesis_k);
    require_expression(tmp);
    require_token(close_parenthesis_k);
    if (!is_statement()) throw_exception("statement required.");
    if (is_keyword(else_k)) {
        if (!is_statement()) throw_exception("statement required.");
    }
    return true;
}

/*************************************************************************************************/
//  statement_while             = "while" "(" expression ")" statement.
bool expression_parser::is_statement_while()
{
    array_t tmp;

    if (!is_keyword(while_k)) return false;
    require_token(open_parenthesis_k);
    require_expression(tmp);
    require_token(close_parenthesis_k);
    if (!is_statement()) throw_exception("statement required.");
    return true;
}

/*************************************************************************************************/
//  statement_do                = "do" statement "while" "(" expression ")" ";".
bool expression_parser::is_statement_do()
{
    array_t tmp;

    if (!is_keyword(do_k)) return false;
    if (!is_statement()) throw_exception("statement required.");
    require_keyword(while_k);
    require_token(open_parenthesis_k);
    require_expression(tmp);
    require_token(close_parenthesis_k);
    require_token(semicolon_k);
    return true;
}

/*************************************************************************************************/
//  statement_compound          = "{" { statement } "}".
bool expression_parser::is_statement_compound()
{
    if (!is_token(open_brace_k)) return false;
    while (is_statement()) ;
    require_token(close_brace_k);
    return true;
}

/*************************************************************************************************/
//  statement_switch            = "switch" "(" expression ")" "{" { statement_case } "}".
bool expression_parser::is_statement_switch()
{
    array_t tmp;

    if (!is_keyword(switch_k)) return false;
    require_token(open_parenthesis_k);
    require_expression(tmp);
    require_token(close_parenthesis_k);
    require_token(open_brace_k);
    while (is_statement_case());
    require_token(close_brace_k);
    return true;
}

/*************************************************************************************************/
//  statement_case              = "case" expression ":" { statement }.
bool expression_parser::is_statement_case()
{
    array_t tmp;

    if (!is_keyword(case_k)) return false;
    require_expression(tmp);
    require_token(colon_k);
    while (is_statement()) ;
    return true;
}

/*************************************************************************************************/
//  statement_goto              = "goto" identifier ";"
bool expression_parser::is_statement_goto()
{
    if (!is_keyword(goto_k)) return false;
    require_identifier();
    require_token(semicolon_k);
    return true;
}

/*************************************************************************************************/
//  expression                  = expression_and { "||" expression_and }.
bool expression_parser::is_expression(array_t& expression_stack)
{
    if (!is_expression_and(expression_stack)) return false;
    
    while (is_token(or_k))
        {
        array_t operand2;
        if (!is_expression_and(operand2)) throw_exception("expression_and required.");
        push_back(expression_stack, operand2);
        expression_stack.push_back(any_regular_t(or_k));
        }
    
    return true;
}

void expression_parser::require_expression(array_t& expression_stack)
{
    if (!is_expression(expression_stack))
    {
         throw_exception("expression required.");
    }
}

/*************************************************************************************************/

//  expression_and              = expression_equality { "&&" expression_equality }.
bool expression_parser::is_expression_and(array_t& expression_stack)
    {
    if (!is_expression_equality(expression_stack)) return false;
    
    while (is_token(and_k))
        {
        array_t operand2;
        if (!is_expression_equality(operand2)) throw_exception("expression_bit_and required.");
        push_back(expression_stack, operand2);
        expression_stack.push_back(any_regular_t(and_k));
        }
    
    return true;
    }

/*************************************************************************************************/

//  expression_equality = expression_relational { ("==" | "!=") expression_relational }.
bool expression_parser::is_expression_equality(array_t& expression_stack)
    {
    if (!is_expression_relational(expression_stack)) return false;
    
    bool is_equal = false;
    while ((is_equal = is_token(equal_k)) || is_token(not_equal_k))
        {
        if (!is_expression_relational(expression_stack)) throw_exception("Primary required.");
        expression_stack.push_back(is_equal ? any_regular_t(equal_k) : any_regular_t(not_equal_k));
        }
    
    return true;
    }

/*************************************************************************************************/

//  expression_relational       = expression_additive { ("<" | ">" | "<=" | ">=") expression_additive }.
bool expression_parser::is_expression_relational(array_t& expression_stack)
{
    if (!is_expression_additive(expression_stack)) return false;
    
    name_t operator_l;
    
    while (is_relational_operator(operator_l))
        {
        if (!is_expression_additive(expression_stack)) throw_exception("expression_shift required.");
        expression_stack.push_back(any_regular_t(operator_l));
        }
    
    return true;
}
/*************************************************************************************************/
//  expression_additive = expression_multiplicative { additive_operator expression_multiplicative }.
bool expression_parser::is_expression_additive(array_t& expression_stack)
    {
    if (!is_expression_multiplicative(expression_stack)) return false;
    
    name_t operator_l;
    
    while (is_additive_operator(operator_l))
        {
        if (!is_expression_multiplicative(expression_stack)) throw_exception("Primary required.");
        expression_stack.push_back(any_regular_t(operator_l));

        }
    
    return true;
    }

/*************************************************************************************************/
//  expression_multiplicative = expression_unary { ("*" | "/" | "%") expression_unary }.
bool expression_parser::is_expression_multiplicative(array_t& expression_stack)
    {
    if (!is_expression_unary(expression_stack)) return false;
    
    name_t operator_l;
    
    while (is_multiplicative_operator(operator_l))
        {
        if (!is_expression_unary(expression_stack)) throw_exception("Primary required.");
        expression_stack.push_back(any_regular_t(operator_l));

        }
    
    return true;
    }

/*************************************************************************************************/
//  expression_unary = expression_postfix | ( ("+" | "-" | "!" | "*" | "&" | "const") expression_unary).
bool expression_parser::is_expression_unary(array_t& expression_stack)
    {
    if (is_expression_postfix(expression_stack)) return true;
    
    name_t operator_l;
    
    if (is_unary_operator(operator_l))
        {
        if (!is_expression_unary(expression_stack)) throw_exception("Unary expression required.");
        if (operator_l != add_k)
            {
            expression_stack.push_back(any_regular_t(operator_l));

            }
        return true;
        }
        
    return false;
    }
    
/*************************************************************************************************/
//  expression_postfix          = expression_primary { ("[" expression "]") | ("." identifier) | ("(" [expression_list] ")") | "&" }.
bool expression_parser::is_expression_postfix(array_t& expression_stack)
    {
    if (!is_expression_primary(expression_stack)) return false;
    
    while (true)
        {
        if (is_token(open_bracket_k))
            {
            require_expression(expression_stack);
            require_token(close_bracket_k);
            }
        else if (is_token(dot_k))
            {
            any_regular_t result;
            require_identifier(result);
            expression_stack.push_back(result);
            }
        else if (is_token(open_parenthesis_k))
            {
            is_expression_list(expression_stack);
            require_token(close_parenthesis_k);
            }
        else if (is_token(reference_k)) { }
        else break;
        
        expression_stack.push_back(any_regular_t(index_k));

        }
    
    return true;
    }
    
/*************************************************************************************************/
//  expression_primary          = number | "true" | "false" | string | identifier | "typename"
//                                  | expression_template | ("(" expression ")").

bool expression_parser::is_expression_primary(array_t& expression_stack)
    {
    any_regular_t result; // empty result used if is_keyword(empty_k)
    
    if (is_token(number_k, result)
            || is_boolean(result)
            || is_token(string_k, result)
            || is_identifier(result))
        {
        expression_stack.push_back(move(result));
        return true;
        }
    if (is_keyword(typename_k)) return true;
    if (is_expression_template()) return true;
    if (is_token(open_parenthesis_k)) {
        require_expression(expression_stack);
        require_token(close_parenthesis_k);
        return true;
    }
    
    return false;
    }
    
/*************************************************************************************************/
//  expression_template         = class_name [ "<" expression_list ">" ].
bool expression_parser::is_expression_template()
{
    bool    is_template;
    name_t  class_name;

    if (!is_class_name(class_name, is_template)) return false;
    if (is_template) {
        if (!is_token(less_k)) return true;
        if (!is_expression_additive_list()) throw_exception("expression_additive_list required.");
        require_token(greater_k);
    }
    return true;
}

/*************************************************************************************************/
//  expression_additive_list    = expression_additive { "," expression_additive }.
bool expression_parser::is_expression_additive_list()
{
    array_t tmp;

    if (!is_expression_additive(tmp)) return false;
        
    while (is_token(comma_k))
    {
        if (!is_expression_additive(tmp)) throw_exception("expression_additive required.");
    }
    return true;
}

/*************************************************************************************************/
//  expression_list = expression { "," expression }.
bool expression_parser::is_expression_list(array_t& expression_stack)
{
    if (!is_expression(expression_stack)) return false;
    
    std::size_t count = 1;
    
    while (is_token(comma_k))
    {
        require_expression(expression_stack);
        ++count;
    }
    
    expression_stack.push_back(any_regular_t(count));
    expression_stack.push_back(any_regular_t(array_k));
    
    return true;
}

/*************************************************************************************************/

bool expression_parser::is_boolean(any_regular_t& result)
    {
    if (is_keyword(true_k))
        { result = any_regular_t(true); return true; }
    else if (is_keyword(false_k))
        { result = any_regular_t(false); return true; }
    
    return false;
    }

/*************************************************************************************************/

//  relational_operator = "<" | ">" | "<=" | ">=".
bool expression_parser::is_relational_operator(name_t& name_result)
    {
    const stream_lex_token_t& result (get_token());
    
    name_t name = result.first;
    if (name == less_k || name == greater_k || name == less_equal_k || name == greater_equal_k)
        {
        name_result = name;
        return true;
        }
    putback();
    return false;
    }

/*************************************************************************************************/
bool expression_parser::is_operator_shift(name_t& name_result)
{
    const stream_lex_token_t& result (get_token());
    
    name_t name = result.first;
    if (name == shift_left_k || name == shift_right_k)
        {
        name_result = name;
        return true;
        }
    putback();
    return false;
}

/*************************************************************************************************/

//  additive_operator = "+" | "-".
bool expression_parser::is_additive_operator(name_t& name_result)
    {
    const stream_lex_token_t& result (get_token());
    
    name_t name = result.first;
    if (name == add_k || name == subtract_k)
        {
        name_result = name;
        return true;
        }
    putback();
    return false;
    }

/*************************************************************************************************/

//  multiplicative_operator = "*" | "/" | "%".
bool expression_parser::is_multiplicative_operator(name_t& name_result)
    {
    const stream_lex_token_t& result (get_token());
    
    name_t name = result.first;
    if (name == multiply_k || name == divide_k || name == modulus_k)
        {
        name_result = name;
        return true;
        }
    putback();
    return false;
    }
    
/*************************************************************************************************/
    
//  unary_operator = "+" | "-" | "!" | "*" | "&" | "const".
bool expression_parser::is_unary_operator(name_t& name_result)
    {
    const stream_lex_token_t& result (get_token());
    
    name_t name = result.first;
    if (name == subtract_k)
        {
        name_result = unary_negate_k;
        return true;
        }
    else if (name == not_k || name == add_k || name == multiply_k /* || name == reference_k */)
        {
        name_result = name;
        return true;
        }
    else if (name == keyword_k && result.second.cast<name_t>() == const_k) {
        name_result = const_k;
        return true;
    }
    putback();
    return false;
    }
    
/*************************************************************************************************/

/*
    REVISIT (sparent) : There should be a turn the simple lexical calls into templates
*/

bool expression_parser::is_identifier(any_regular_t& result)
{
    const stream_lex_token_t& token = get_token();
    
    if (token.first == identifier_k)
    {
        if (!object->class_name_index_m.count(token.second.cast<adobe::name_t>())) {
            result = token.second;
            return true;
        }
    }
    
    putback();
    return false;
}

void expression_parser::require_identifier(any_regular_t& result)
{
    if (!is_identifier(result)) throw_exception("identifier required.");
}

bool expression_parser::is_identifier(name_t& name_result)
{
    const stream_lex_token_t& result (get_token());
    
    if (result.first == identifier_k)
    {
        name_result = result.second.cast<adobe::name_t>();
        if (!object->class_name_index_m.count(name_result)) return true;
    }
    
    putback();
    return false;
}

bool expression_parser::is_identifier()
{
    name_t tmp;
    return is_identifier(tmp);
}

void expression_parser::require_identifier()
{
    if (!is_identifier()) throw_exception("identifier required.");
}


bool expression_parser::is_lead_comment(std::string& string_result)
{
    const stream_lex_token_t& result (get_token());
    
    if (result.first == lead_comment_k)
    {
        string_result = result.second.cast<std::string>();
        return true;
    }
    
    putback();
    return false;
}

bool expression_parser::is_trail_comment(std::string& string_result)
{
    const stream_lex_token_t& result (get_token());
    
    if (result.first == trail_comment_k)
    {
        string_result = result.second.cast<std::string>();
        return true;
    }
    
    putback();
    return false;
}
    
/*************************************************************************************************/

bool expression_parser::is_token(name_t tokenName, any_regular_t& tokenValue)
    {
    const stream_lex_token_t& result (get_token());
    if (result.first == tokenName)
        {
        tokenValue = result.second;
        return true;
        }
    putback();
    return false;
    }

/*************************************************************************************************/

bool expression_parser::is_token(name_t tokenName)
    {
    const stream_lex_token_t& result (get_token());
    if (result.first == tokenName)
        {
        return true;
        }
    putback();
    return false;
    }

/*************************************************************************************************/

bool expression_parser::is_keyword(name_t keyword_name)
{
    const stream_lex_token_t& result (get_token());
    if (result.first == keyword_k && result.second.cast<name_t>() == keyword_name) return true;
    putback();
    return false;
}

/*************************************************************************************************/

const stream_lex_token_t& expression_parser::get_token()
{
    return object->token_stream_m.get();
}

/*************************************************************************************************/

void expression_parser::putback()
{
    object->token_stream_m.putback();
}

/*************************************************************************************************/

void expression_parser::require_token(name_t tokenName, any_regular_t& tokenValue)
    {
    const stream_lex_token_t& result (get_token());
    if (result.first != tokenName)
        {
        putback();
        throw_exception(tokenName, result.first);
        }

    tokenValue = result.second;
    }

/*************************************************************************************************/

void expression_parser::require_keyword(name_t keyword_name)
{
    const stream_lex_token_t& result (get_token());
    name_t result_name;
    name_t result_token = result.first;
    if (result_token == keyword_k) {
        result_name = result.second.cast<name_t>();
        if (result_name == keyword_name) return;
        putback();
        throw_exception(keyword_name, result_name);
    }
    putback();
    throw_exception(keyword_name, result_token);
}

/*************************************************************************************************/

void expression_parser::require_token(name_t tokenName)
{
    const stream_lex_token_t& result (get_token());
    if (result.first == tokenName) return;

    putback();
    throw_exception(tokenName, result.first);
}

/*************************************************************************************************/

} // namespace eop

/*************************************************************************************************/
