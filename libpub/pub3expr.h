// -*-c++-*-

#pragma once

#include "pub3base.h"
#include "pub3prot.h"
#include "pscalar.h"

namespace pub3 {

  //-----------------------------------------------------------------------

  // See pub3eval.h for a definition of eval_t; but don't included it
  // here to prevent circular inclusions.
  class eval_t;
  class publish_t;
  class mref_t;

  //-----------------------------------------------------------------------

  class expr_t;
  class expr_regex_t;
  class expr_assignment_t;
  class expr_dict_t;
  class expr_list_t;
  class bindtab_t;
  class proc_call_t; // declared in pub3ast.h

  //-----------------------------------------------------------------------

  typedef event<ptr<const expr_t> >::ref cxev_t;
  typedef event<ptr<mref_t> >::ref mrev_t;

  //-----------------------------------------------------------------------

  class json {
  public:
    static str null() { return _null; }
    static str quote (const str &s);
    static str safestr (const str &s);
    static str _null;
  };

  //-----------------------------------------------------------------------

  class expr_t : virtual public refcount {
  public:
    expr_t (lineno_t lineno = 0) : _lineno (lineno) {}
    virtual ~expr_t () {}

    enum { max_stack_depth = 128,
	   max_shell_strlen = 0x100000 };

    virtual bool to_xdr (xpub3_expr_t *x) const = 0;
    virtual bool to_xdr (xpub3_json_t *x) const;

    static ptr<expr_t> alloc (const xpub3_expr_t &x);
    static ptr<expr_t> alloc (const xpub3_expr_t *x);
    static ptr<expr_t> alloc (const xpub3_json_t *x);
    static ptr<expr_t> alloc (const xpub3_json_t &x);

    static ptr<expr_t> alloc (scalar_obj_t so);
    static ptr<expr_t> safe_expr (ptr<expr_t> in);
    lineno_t lineno () const { return _lineno; }
    static str safe_to_str (ptr<const expr_t> x, bool q = true);

    //------- Evaluation ------------------------------------------
    //
    virtual ptr<const expr_t> eval_to_val (eval_t e) const;
    virtual ptr<mref_t> eval_to_ref (eval_t e) const;
    //
    //------------------------------------------------------------

    virtual ptr<expr_t> copy () const ;
    virtual ptr<expr_t> deep_copy () const;
    virtual ptr<expr_t> mutate () { return mkref (this); }

    virtual bool is_static () const { return false; }
    bool might_block () const;
    virtual bool might_block_uncached () const { return false; }
    static bool might_block (ptr<const expr_t> x1, ptr<const expr_t> x2 = NULL);
    
    //------------------------------------------------------------

    //------------------------------------------------------------
    // shortcuts

    virtual bool eval_as_bool (eval_t e) const;
    virtual bool eval_as_null (eval_t e) const;
    virtual scalar_obj_t eval_as_scalar (eval_t e) const;
    virtual str eval_as_str (eval_t e) const;
    virtual ptr<const expr_dict_t> eval_as_dict (eval_t e) const;

    //
    //------------------------------------------------------------

    virtual void pub_to_val (publish_t pub, cxev_t ev, CLOSURE) const;
    virtual void pub_to_ref (publish_t pub, mrev_t ev, CLOSURE) const;
    void pub_as_bool (publish_t pub, evb_t ev, CLOSURE) const;
    void pub_as_null (publish_t pub, evb_t ev, CLOSURE) const;

    //------------------------------------------------------------
    //
    // Once objects are evaluated, they can be turned into....

    virtual ptr<const expr_dict_t> to_dict () const { return NULL; }
    virtual ptr<expr_dict_t> to_dict () { return NULL; }
    virtual ptr<const expr_list_t> to_list () const { return NULL; }
    virtual ptr<expr_list_t> to_list () { return NULL; }

    virtual str to_identifier () const { return NULL; }
    virtual str to_str (bool q = false) const { return NULL; }
    virtual str to_switch_str () const { return to_str (); }
    virtual bool to_bool () const { return false; }
    virtual int64_t to_int () const { return 0; }
    virtual u_int64_t to_uint () const { return 0; }
    virtual bool to_uint (u_int64_t *u) const { return false; }
    virtual bool to_len (size_t *s) const { return false; }
    virtual bool is_null () const { return false; }
    virtual ptr<rxx> to_regex () const { return NULL; }
    virtual ptr<expr_regex_t> to_regex_obj () { return NULL; }
    virtual str type_to_str () const { return "object"; }
    virtual bool to_int (int64_t *out) const { return false; }
    virtual scalar_obj_t to_scalar () const;
    virtual ptr<const proc_call_t> to_proc_call () const { return NULL; }
    virtual ptr<proc_call_t> to_proc_call () { return NULL; }

    //
    // and from here, scalars can be converted at will...
    //
    //-----------------------------------------------------------

    //-----------------------------------------------------------
    // One can push arguments onto a fair number of different objects
    // (like idenitifers, runtime functions, and in the future, references
    // to function (like function pointers). 
    // 

    virtual bool unshift_argument (ptr<expr_t> e) { return false; }

    //
    //-------------------------------------------------------------
    
    void report_error (eval_t e, str n) const;
    
  protected:
    ptr<rxx> str2rxx (const eval_t *e, const str &b, const str &o) const;
    lineno_t _lineno;
    mutable tri_bool_t _might_block;
  };
  
  //-----------------------------------------------------------------------

  class mref_t {
  public:
    mref_t () {}
    virtual ~mref_t () {}
    virtual ptr<expr_t> get_value () = 0;
    virtual bool set_value (ptr<expr_t> x) = 0;
  };

  //----------------------------------------------------------------------

  class expr_cow_t : public expr_t {
  public:
    expr_cow_t (ptr<const expr_t> x) : _orig (x) {}
    static ptr<expr_cow_t> alloc (ptr<const expr_t> x);
    ptr<expr_dict_t> to_dict ();
    ptr<const expr_dict_t> to_dict () const;
    ptr<expr_list_t> to_list ();
    ptr<const expr_list_t> to_list () const;
    bool to_xdr (xpub3_expr_t *x) const;
    bool is_static () const;
    bool might_block_uncached () const;
    
  protected:
    ptr<expr_t> mutable_ptr ();
    ptr<const expr_t> const_ptr () const;

    ptr<expr_t> _copy;
    ptr<const expr_t> _orig;
  };

  //----------------------------------------------------------------------

  class const_mref_t : public mref_t {
  public:
    const_mref_t (ptr<expr_t> x) : mref_t (), _x (x) {}
    ptr<expr_t> get_value () { return _x; }
    bool set_value (ptr<expr_t> x) { return false; }
    static ptr<const_mref_t> alloc (ptr<expr_t> x)
    { return New refcounted<const_mref_t> (x); }
  protected:
    ptr<expr_t> _x;
  };

  //----------------------------------------------------------------------

  class mref_dict_t : public mref_t {
  public:
    static ptr<mref_dict_t> alloc (ptr<bindtab_t> d, const str &n);
    mref_dict_t (ptr<bindtab_t> d, const str &n) : _dict (d), _slot (n) {}
    ptr<expr_t> get_value ();
    bool set_value (ptr<expr_t> x);
  protected:
    const ptr<bindtab_t> _dict;
    const str _slot;
  };

  //----------------------------------------------------------------------

  class const_mref_dict : public mref_t {


  };

  //----------------------------------------------------------------------

  class mref_list_t : public mref_t {
  public:
    mref_list_t (ptr<expr_list_t> l, ssize_t i) : _list (l), _index (i) {}
    static ptr<mref_list_t> alloc (ptr<expr_list_t> d, ssize_t i);
    ptr<expr_t> get_value ();
    bool set_value (ptr<expr_t> x);
  protected:
    const ptr<expr_list_t> _list;
    const ssize_t _index;
  };

  //----------------------------------------------------------------------

  class expr_constant_t : public expr_t {
  public:
    expr_constant_t () : expr_t () {}
    expr_constant_t (lineno_t l) : expr_t (l) {}
    ptr<expr_t> copy () const;
    ptr<expr_t> deep_copy () const;
    bool is_static () const { return true; }
  };

  //----------------------------------------------------------------------

  class expr_null_t : public expr_constant_t {
  public:
    expr_null_t () : expr_constant_t () {}
    bool is_null () const { return true; }
    bool to_xdr (xpub3_expr_t *x) const ;
    bool to_xdr (xpub3_json_t *x) const;
    const char *get_obj_name () const { return "pub3::expr_null_t"; }
    static ptr<expr_null_t> alloc ();
    str type_to_str () const { return "null"; }
  };

  //-----------------------------------------------------------------------

  class expr_bool_t : public expr_constant_t {
  public:
    scalar_obj_t to_scalar () const;
    bool to_xdr (xpub3_expr_t *x) const;
    const char *get_obj_name () const { return "pub3::expr_bool_t"; }
    bool to_xdr (xpub3_json_t *j) const;
    static ptr<expr_bool_t> alloc (bool b);
    static str static_to_str (bool b);
    str to_str (bool q = false) const;
    str to_switch_str () const { return _b ? "1" : "0"; }
    ptr<expr_t> copy () const;

    // Allow for calling New refcounted<expr_bool_t> inside of
    // expr_bool_t::alloc()
    friend class refcounted<expr_bool_t>;
    str type_to_str () const { return "bool"; }

  private:
    static ptr<expr_bool_t> _false, _true;
    expr_bool_t (bool b) : expr_constant_t (), _b (b) {}
    const bool _b;
  };

  //-----------------------------------------------------------------------

  class expr_logical_t : public expr_t {
  public:
    expr_logical_t (lineno_t lineno) : expr_t (lineno) {}
    ptr<const expr_t> eval_to_val (eval_t e) const;
    void pub_to_val (publish_t p, cxev_t ev, CLOSURE) const;
  protected:
    virtual bool eval_logical (eval_t e) const = 0;
    virtual void pub_logical (publish_t p, evb_t, CLOSURE) const = 0;
  };

  //-----------------------------------------------------------------------

  class expr_OR_t : public expr_logical_t {
  public:
    expr_OR_t (ptr<expr_t> t1, ptr<expr_t> t2, lineno_t l) 
      : expr_logical_t (l), _t1 (t1), _t2 (t2) {}
    expr_OR_t (const xpub3_mathop_t &x);
    static ptr<expr_OR_t> alloc (ptr<expr_t> t1, ptr<expr_t> t2);
    bool to_xdr (xpub3_expr_t *x) const;
    const char *get_obj_name () const { return "pub3::expr_OR_t"; }
    bool might_block_uncached () const { return might_block (_t1, _t2); }
  protected:
    bool eval_logical (eval_t e) const;
    void pub_logical (publish_t p, evb_t, CLOSURE) const;
    ptr<expr_t> _t1, _t2;
  };

  //-----------------------------------------------------------------------

  class expr_AND_t : public expr_logical_t  {
  public:
    expr_AND_t (ptr<expr_t> f1, ptr<expr_t> f2, lineno_t lineno) 
      : expr_logical_t (lineno), _f1 (f1), _f2 (f2) {}

    static ptr<expr_AND_t> alloc (ptr<expr_t> t1, ptr<expr_t> t2);
    expr_AND_t (const xpub3_mathop_t &x);
    bool to_xdr (xpub3_expr_t *x) const;
    const char *get_obj_name () const { return "pub3::expr_AND_t"; }
  protected:
    ptr<expr_t> _f1, _f2;
    bool eval_logical (eval_t e) const;
    void pub_logical (publish_t p, evb_t, CLOSURE) const;
  };

  //-----------------------------------------------------------------------

  class expr_NOT_t : public expr_logical_t  {
  public:
    expr_NOT_t (ptr<expr_t> e, lineno_t lineno) 
      : expr_logical_t (lineno), _e (e) {}
    static ptr<expr_NOT_t> alloc (ptr<expr_t> x);

    expr_NOT_t (const xpub3_not_t &x);
    bool to_xdr (xpub3_expr_t *x) const;
    const char *get_obj_name () const { return "pub3::expr_NOT_t"; }
  protected:
    ptr<expr_t> _e;
    bool eval_logical (eval_t e) const;
    void pub_logical (publish_t p, evb_t, CLOSURE) const;
  };

  //-----------------------------------------------------------------------

  class expr_EQ_t : public expr_logical_t {
  public:
    expr_EQ_t (ptr<expr_t> o1, ptr<expr_t> o2, bool pos, lineno_t ln) 
      : expr_logical_t (ln), _o1 (o1), _o2 (o2), _pos (pos) {}
    expr_EQ_t (const xpub3_eq_t &x);
    static ptr<expr_EQ_t> alloc (ptr<expr_t> o1, ptr<expr_t> o2, bool pos);

    bool to_xdr (xpub3_expr_t *x) const;
    const char *get_obj_name () const { return "pub3::expr_EQ_t"; }
  protected:
    ptr<expr_t> _o1, _o2;
    bool _pos;
    bool eval_logical (eval_t e) const;
    void pub_logical (publish_t p, evb_t, CLOSURE) const;
    bool eval_final (ptr<const expr_t> x1, ptr<const expr_t> x2) const;
  };

  //-----------------------------------------------------------------------

  class expr_relation_t : public expr_logical_t {
  public:
    expr_relation_t (ptr<expr_t> l, ptr<expr_t> r, xpub3_relop_t op, 
		     lineno_t lineno)
      : expr_logical_t (lineno), _l (l), _r (r), _op (op) {}
    expr_relation_t (const xpub3_relation_t &x);

    static ptr<expr_relation_t> alloc (ptr<expr_t> l, ptr<expr_t> r,
				       xpub3_relop_t op);

    bool to_xdr (xpub3_expr_t *x) const;
    const char *get_obj_name () const { return "pub3::expr_relation_t"; }

  protected:
    ptr<expr_t> _l, _r;
    xpub3_relop_t _op;
    bool eval_logical (eval_t e) const;
    void pub_logical (publish_t pub, evb_t ev, CLOSURE) const;
    bool eval_final (eval_t e, ptr<const expr_t> l, ptr<const expr_t> r) const;
  };

  //-----------------------------------------------------------------------

  // a convenience class for moving to and from XDR representation
  class expr_mathop_t {
  public:
    static ptr<expr_t> alloc (const xpub3_mathop_t &x);
    static bool to_xdr (xpub3_expr_t *x, xpub3_mathop_opcode_t typ,
			const expr_t *o1, const expr_t *o2, lineno_t lineno);
  };

  //-----------------------------------------------------------------------

  class expr_binaryop_t : public expr_t {
  public:
    expr_binaryop_t (ptr<expr_t> o1, ptr<expr_t> o2, lineno_t lineno)
      : expr_t (lineno), _o1 (o1), _o2 (o2) {}

    ptr<const expr_t> eval_to_val (eval_t e) const;
    void pub_to_val (publish_t pub, cxev_t ev, CLOSURE);
    bool might_block_uncached () const;
    bool to_xdr (xpub3_expr_t *x) const;

  protected:
    virtual ptr<const expr_t> 
    eval_final (eval_t e, ptr<const expr_t> o1, ptr<const expr_t> o2) const = 0;

    virtual xpub3_mathop_opcode_t opcode () const = 0;

    ptr<expr_t> _o1, _o2;
  };

  //-----------------------------------------------------------------------

  class expr_add_t : public expr_binaryop_t {
  public:
    expr_add_t (ptr<expr_t> t1, ptr<expr_t> t2, bool pos, lineno_t lineno)
      : expr_binaryop_t (t1, t2, lineno), _pos (pos) {}
    expr_add_t (const xpub3_mathop_t &x);
    static ptr<expr_add_t> alloc (ptr<expr_t> t1, ptr<expr_t> t2, bool pos);
    bool to_xdr (xpub3_expr_t *x) const;
    const char *get_obj_name () const { return "pub3::expr_add_t"; }

  protected:
    ptr<const expr_t> eval_final (eval_t e, ptr<const expr_t> o1, 
				  ptr<const expr_t> o2) const;
    xpub3_mathop_opcode_t opcode () const;
    bool _pos;
  };

  //-----------------------------------------------------------------------

  class expr_mult_t : public expr_binaryop_t {
  public:
    expr_mult_t (ptr<expr_t> f1, ptr<expr_t> f2, lineno_t lineno)
      : expr_binaryop_t (f1, f2, lineno) {}
    expr_mult_t (const xpub3_mathop_t &x);
    static ptr<expr_mult_t> alloc (ptr<expr_t> l, ptr<expr_t> r);
    const char *get_obj_name () const { return "pub3::expr_mult_t"; }
  protected:
    ptr<const expr_t> eval_final (eval_t e, ptr<const expr_t> o1, 
				  ptr<const expr_t> o2) const;
    xpub3_mathop_opcode_t opcode () const;
  };

  //-----------------------------------------------------------------------

  class expr_div_or_mod_t : public expr_binaryop_t {
  public:
    expr_div_or_mod_t (ptr<expr_t> n, ptr<expr_t> d, lineno_t lineno)
      : expr_binaryop_t (n, d, lineno) {}

  protected:
    ptr<const expr_t> eval_final (eval_t e, ptr<const expr_t> o1, 
				  ptr<const expr_t> o2) const;

    virtual bool div () const = 0;
    virtual const char *operation () const = 0;
  };

  //-----------------------------------------------------------------------

  class expr_div_t : public expr_div_or_mod_t {
  public:
    expr_div_t (ptr<expr_t> n, ptr<expr_t> d, lineno_t lineno)
      : expr_div_or_mod_t (n, d, lineno) {}
    expr_div_t (const xpub3_mathop_t &x);
    static ptr<expr_div_t> alloc (ptr<expr_t> n, ptr<expr_t> d);

    const char *get_obj_name () const { return "pub3::expr_div_t"; }
  protected:
    bool div () const { return true; }
    const char *operation () const { return "division"; }
    xpub3_mathop_opcode_t opcode () const;
  };

  //-----------------------------------------------------------------------

  class expr_mod_t : public expr_div_or_mod_t {
  public:
    expr_mod_t (ptr<expr_t> n, ptr<expr_t> d, lineno_t lineno)
      : expr_div_or_mod_t (n, d, lineno) {}
    expr_mod_t (const xpub3_mathop_t &x);
    static ptr<expr_mod_t> alloc (ptr<expr_t> n, ptr<expr_t> d);

    bool to_xdr (xpub3_expr_t *x) const;
    const char *get_obj_name () const { return "pub3::expr_add_t"; }
  protected:
    bool div () const { return false; }
    xpub3_mathop_opcode_t opcode () const;
    const char *operation () const { return "modulo"; }
  };

  //-----------------------------------------------------------------------

  class expr_dictref_t : public expr_t {
  public:
    expr_dictref_t (ptr<expr_t> d, const str &k, lineno_t lineno)
      : expr_t (lineno), _dict (d), _key (k) {}
    expr_dictref_t (const xpub3_dictref_t &x);
    static ptr<expr_dictref_t> alloc (ptr<expr_t> d, const str &k);
    bool to_xdr (xpub3_expr_t *x) const;
    const char *get_obj_name () const { return "pub3::expr_dictref_t"; }

    ptr<const expr_t> eval_to_val (eval_t e) const;
    ptr<mref_t> eval_to_ref (eval_t e) const;
    void pub_to_ref (publish_t pub, mrev_t ev, CLOSURE) const;
    void pub_to_val (publish_t pub, cxev_t ev, CLOSURE) const;
  protected:
    bool might_block_uncached () const;
    ptr<mref_t> eval_to_ref_final (eval_t e, ptr<mref_t> dr) const;
    ptr<const expr_t> eval_to_val_final (eval_t e, ptr<const expr_t> d) const;
    ptr<expr_t> _dict;
    str _key;
  };

  //-----------------------------------------------------------------------

  class expr_varref_t : public expr_t {
  public:
    expr_varref_t (const str &s, lineno_t l) : expr_t (l), _name (s) {}
    expr_varref_t (const xpub3_ref_t &x);
    virtual bool to_xdr (xpub3_expr_t *x) const;
    str to_identifier () const { return _name; }
    virtual const char *get_obj_name () const { return "pub3::expr_varref_t"; }
    static ptr<expr_varref_t> alloc (const str &n);

    ptr<const expr_t> eval_to_val (eval_t e) const;
    ptr<mref_t> eval_to_ref (eval_t e) const;

  protected:
    str _name;
  };

  //-----------------------------------------------------------------------

  class expr_vecref_t : public expr_t {
  public:
    expr_vecref_t (ptr<expr_t> v, ptr<expr_t> i, lineno_t l) 
      : expr_t (l), _vec (v), _index (i) {}
    expr_vecref_t (const xpub3_vecref_t &x);

    bool to_xdr (xpub3_expr_t *x) const;
    const char *get_obj_name () const { return "pub3::expr_vecref_t"; }
    static ptr<expr_vecref_t> alloc (ptr<expr_t> v, ptr<expr_t> i);

    ptr<const expr_t> eval_to_val (eval_t e) const;
    ptr<mref_t> eval_to_ref (eval_t e) const;
    void pub_to_val (publish_t p, cxev_t ev, CLOSURE) const;
    void pub_to_ref (publish_t p, mrev_t ev, CLOSURE) const;
  protected:
    bool might_block_uncached () const;
    ptr<const expr_t> eval_to_val_final (eval_t e, ptr<const expr_t> container,
					 ptr<const expr_t> index) const;
    ptr<mref_t> eval_to_ref_final (eval_t e, ptr<mref_t> cr,
				   ptr<const expr_t> i) const;

    ptr<expr_t> _vec;
    ptr<expr_t> _index;
  };
    
  //-----------------------------------------------------------------------

  // The parser doesn't know at allocation time if a variable reference
  // is a variable reference or rather a function in a pipeline. 
  // Therefore, this class is used in conjunction with the parser
  // to capture that dual-nature/uncertainty.  NOTE that we'll never
  // need to ship one of these over the wire via the xpub protocol,
  // since by that time, the true nature of this beast is known.
  class expr_varref_or_rfn_t : public expr_varref_t {
  public:
    expr_varref_or_rfn_t (const str &s, lineno_t l) : expr_varref_t (s, l) {}
    bool to_xdr (xpub3_expr_t *x) const;
    bool unshift_argument (ptr<expr_t> e);
    static ptr<expr_varref_or_rfn_t> alloc (const str &l);

    const char *get_obj_name () const { return "pub3::expr_varref_or_rfn_t"; }

    ptr<const expr_t> eval_to_val (eval_t e) const;
    ptr<mref_t> eval_to_ref (eval_t e) const;
    void pub_to_val (publish_t p, cxev_t ev, CLOSURE) const;
    void pub_to_ref (publish_t p, mrev_t ev, CLOSURE) const;


  protected:
    bool might_block_uncached () const { return true; }
    ptr<const expr_t> get_rfn () const;
    ptr<expr_list_t> _arglist;
    mutable ptr<expr_t> _rfn;
  };

  //-----------------------------------------------------------------------

  class expr_strbuf_t : public expr_constant_t {
  public:
    expr_strbuf_t (const str &s = NULL, lineno_t l = 0) 
      : expr_constant_t (l) { if (s) add (s); }

    str to_str (bool q = false) const;
    bool to_bool () const;
    scalar_obj_t to_scalar () const;
    bool to_null () const;
    ptr<rxx> to_regex () const;
    static ptr<expr_strbuf_t> alloc (const str &s = NULL);

    void add (char ch);
    void add (str s);

    const char *get_obj_name () const { return "pub3::expr_strbuf_t"; }
    bool to_len (size_t *s) const;
    bool to_xdr (xpub3_expr_t *x) const;

  protected:
    vec<str> _hold;
    strbuf _b;
  };

  //-----------------------------------------------------------------------

  class expr_str_t : public expr_constant_t {
  public:
    expr_str_t (str s = NULL) : expr_constant_t(), _val (s) {}
    expr_str_t (const xpub3_str_t &x);

    str to_str (bool q = false) const;
    bool to_bool () const;
    scalar_obj_t to_scalar () const;
    bool to_null () const;
    ptr<rxx> to_regex () const;
    static ptr<expr_str_t> alloc (str s = NULL);
    static ptr<expr_str_t> alloc (const xpub3_json_str_t &x);

    const char *get_obj_name () const { return "pub3::expr_str_t"; }
    bool to_len (size_t *s) const;
    bool to_xdr (xpub3_expr_t *x) const;
    bool to_xdr (xpub3_json_t *j) const;

    int64_t to_int () const;
    u_int64_t to_uint () const;
    bool to_uint (u_int64_t *u) const;
    bool to_int64 (int64_t *u) const;

  protected:
    const str _val;
  };

  //-----------------------------------------------------------------------

  class expr_number_t : public expr_constant_t {
  public:
    expr_number_t () : expr_constant_t () {}

    str to_str () const { return to_scalar ().to_str (); }
  };

  //-----------------------------------------------------------------------

  class expr_int_t : public expr_number_t {
  public:
    expr_int_t (int64_t i) : _val (i) {}
    expr_int_t (const xpub3_int_t &x);

    bool to_bool () const { return _val; }
    scalar_obj_t to_scalar () const;
    int64_t to_int () const { return _val; }
    bool to_int (int64_t *i) const { *i = _val; return true; }
    u_int64_t to_uint () const;
    bool to_uint (u_int64_t *u) const;
    bool to_xdr (xpub3_expr_t *x) const;
    const char *get_obj_name () const { return "pub3::expr_int_t"; }

    static ptr<expr_int_t> alloc (int64_t i);

    // need this only to implement recycling
    void init (int64_t i) { _val = i; }
    void finalize ();

    str type_to_str () const { return "int"; }
    bool to_xdr (xpub3_json_t *j) const;

  private:
    int64_t _val;
  };

  //-----------------------------------------------------------------------

  class expr_uint_t : public expr_number_t {
  public:
    expr_uint_t (u_int64_t i) : _val (i) {}
    expr_uint_t (const xpub3_uint_t &x);

    bool to_bool () const { return _val; }
    u_int64_t to_uint () const { return _val; }
    int64_t to_int () const;
    bool to_int (int64_t *i) const;
    bool to_uint (u_int64_t *u) const { *u = _val; return true; }

    scalar_obj_t to_scalar () const;

    bool to_xdr (xpub3_expr_t *x) const;
    const char *get_obj_name () const { return "pub3::expr_uint_t"; }

    static ptr<expr_uint_t> alloc (u_int64_t i) 
    { return New refcounted<expr_uint_t> (i); }
    bool to_xdr (xpub3_json_t *x) const;
  private:
    const u_int64_t _val;
  };

  //-----------------------------------------------------------------------

  class expr_double_t : public expr_number_t {
  public:
    expr_double_t (double d) : _val (d) {}
    expr_double_t (const xpub3_double_t &d);

    bool to_bool () const { return _val != 0; }
    double to_double () const { return _val; }
    scalar_obj_t to_scalar () const;

    bool to_xdr (xpub3_expr_t *x) const;
    const char *get_obj_name () const { return "pub3::expr_double_t"; }

    static ptr<expr_double_t> alloc (double i) 
    { return New refcounted<expr_double_t> (i); }
    static ptr<expr_double_t> alloc (const xpub3_double_t &x)
    { return New refcounted<expr_double_t> (x); }

    str type_to_str () const { return "float"; }
    bool to_xdr (xpub3_json_t *j) const;
    bool to_xdr (xpub3_double_t *j) const;
  private:
    const double _val;
  };

  //-----------------------------------------------------------------------

  class expr_list_t : public expr_t, 
		      public vec<ptr<expr_t> > {
  public:

    typedef vec<ptr<expr_t> > vec_base_t;

    expr_list_t (lineno_t lineno = -1) : expr_t (lineno) {}
    expr_list_t (const xpub3_expr_list_t &x);

    bool to_xdr (xpub3_expr_t *) const;
    bool to_xdr (xpub3_expr_list_t *) const;
    bool to_xdr (xpub3_json_t *j) const;

    // vec_iface_t interface
    ptr<const expr_t> lookup (ssize_t i, bool *ib = NULL) const;
    ptr<expr_t> lookup (ssize_t i, bool *ib = NULL);
    void set (ssize_t i, ptr<expr_t> v);
    ptr<expr_t> &push_back () { return vec_base_t::push_back (); }
    ptr<expr_t> &push_back (ptr<expr_t> x);
    ptr<expr_t> deep_copy () const;
    
    size_t size () const { return vec_base_t::size (); }
    void setsize (size_t s) { vec_base_t::setsize (s); }
    bool to_len (size_t *s) const;
    bool to_bool () const { return size () > 0; }
    ptr<rxx> to_regex (const eval_t *e = NULL) const;
    scalar_obj_t to_scalar () const;
    str to_str (bool q = false) const;

    void push_front (ptr<expr_t> e);

    static ptr<expr_list_t> alloc ();
    static ptr<expr_list_t> alloc (lineno_t l) 
    { return New refcounted<expr_list_t> (l); }
    static ptr<expr_list_t> alloc (const xpub3_expr_list_t &x) 
    { return New refcounted<expr_list_t> (x); }
    static ptr<expr_list_t> alloc (const xpub3_expr_list_t *x);
    static ptr<expr_list_t> alloc (const xpub3_json_list_t &x);

    ptr<const expr_list_t> to_list () const { return mkref (this); }
    ptr<expr_list_t> to_list () { return mkref (this); }

    const char *get_obj_name () const { return "pub3::expr_list_t"; }

    str type_to_str () const { return "list"; }
    ptr<mref_t> eval_to_ref (eval_t e) const;
    bool is_static () const;
  private:
    bool fixup_index (ssize_t *ind, bool lax = false) const;
    mutable tri_bool_t _static;
  };
  
  //-----------------------------------------------------------------------

  class rxx_factory_t {
  public:
    rxx_factory_t () {}
    static ptr<rxx> compile (str body, str opts, str *err);
  private:
    qhash<str, ptr<rxx> > _cache;
    ptr<rxx> _compile (str body, str opts, str *err);
  };

  //-----------------------------------------------------------------------

  class expr_regex_t : public expr_t {
  public:
    expr_regex_t (lineno_t lineno);
    expr_regex_t (const xpub3_regex_t &x);
    expr_regex_t (ptr<rxx> x, str b, str o, lineno_t lineno);
    const char *get_obj_name () const { return "pub3::expr_regex_t"; }
    bool to_xdr (xpub3_expr_t *x) const;
    static ptr<expr_regex_t> alloc (ptr<rxx>, str b, str o); 

    str to_str () const { return _body; }
    ptr<rxx> to_regex () const { return _rxx; }
    ptr<expr_regex_t> to_regex_obj () { return mkref (this); }
    
    str type_to_str () const { return "regex"; }
    bool is_static () const { return true; }
  private:
    ptr<rxx> _rxx;
    str _body, _opts;
  };

  //-----------------------------------------------------------------------

  class expr_shell_str_t : public expr_t {
  public:
    expr_shell_str_t (lineno_t lineno);
    expr_shell_str_t (const str &s, lineno_t lineno);
    expr_shell_str_t (ptr<expr_t> e, lineno_t lineno);
    expr_shell_str_t (const xpub3_shell_str_t &x);

    static ptr<expr_shell_str_t> alloc (str s = NULL);

    ptr<const expr_t> eval_to_val (eval_t e) const;

    ptr<expr_t> compact () const;
    void add (ptr<expr_t> e) { _els->push_back (e); }
    bool to_xdr (xpub3_expr_t *x) const;
    const char *get_obj_name () const { return "pub3::expr_shell_str_t"; }

    str type_to_str () const { return "string"; }
  protected:
    ptr<expr_list_t> _els;

  private:
    void make_str (strbuf *b, vec<str> *v);
  };

  //-----------------------------------------------------------------------

  class binding_t {
  public:
    binding_t () {}
    binding_t (const str &s, ptr<expr_t> x);
    binding_t (const xpub3_binding_t &x);
    str name () const { return _name; }
    ptr<expr_t> expr () { return _expr; }
    ptr<const expr_t> expr () const { return _expr; }
  private:
    str _name;
    ptr<expr_t> _expr;
  };

  //-----------------------------------------------------------------------

  class bind_interface_t {
  public:
    virtual bool lookup (const str &nm, ptr<const expr_t> *x = NULL)
      const = 0;
    virtual ptr<bindtab_t> mutate () = 0;
  };

  //-----------------------------------------------------------------------

  class bindtab_t : public qhash<str, ptr<expr_t> >,
		    public bind_interface_t,
		    public virtual refcount {
  public:
    void overwrite_with (const bindtab_t &in);
    bindtab_t &operator+= (const bindtab_t &in);
    bindtab_t &operator-= (const bindtab_t &in);
    bool lookup (const str &nm, ptr<const expr_t> *x = NULL) const;
    ptr<bindtab_t> mutate () { return mkref (this); }
    typedef qhash_const_iterator_t<str, ptr<expr_t> > const_iterator_t;
    typedef qhash_iterator_t<str, ptr<expr_t> > iterator_t;
  };

  //-----------------------------------------------------------------------

  class cow_bindtab_t : public bind_interface_t {
  public:
    cow_bindtab_t (ptr<const bindtab_t> t) : _tab (t) {}
    static ptr<cow_bindtab_t> alloc (ptr<const bindtab_t> t);
    bool lookup (const str &nm, ptr<const expr_t> *x = NULL) const;
    ptr<bindtab_t> mutate ();
  protected:
    ptr<const bindtab_t> _tab;
  };

  //-----------------------------------------------------------------------

  class bindlist_t : public vec<binding_t> {
  public:
    bindlist_t (lineno_t l) : _lineno (l) {}
    bindlist_t (const xpub3_bindlist_t &x);
    static ptr<bindlist_t> alloc ();
    void to_xdr (xpub3_bindlist_t *x);
    void add (binding_t b);
  private:
    lineno_t _lineno;
  };

  //-----------------------------------------------------------------------

  class expr_dict_t : public expr_t, public bindtab_t {
  public:
    expr_dict_t (lineno_t lineno = -1) : expr_t (lineno) {}
    expr_dict_t (const xpub3_dict_t &x);

    static ptr<expr_dict_t> parse_alloc ();
    static ptr<expr_dict_t> alloc (const xpub3_json_dict_t &x);
    static ptr<expr_dict_t> alloc ();

    // To JSON-style string
    scalar_obj_t to_scalar () const;
    str to_str (bool q) const;

    ptr<expr_t> lookup (str k);
    ptr<const expr_t> lookup (str k) const;

    bool to_len (size_t *s) const;

    void add (binding_t p);
    void add (ptr<binding_t> p);
    const char *get_obj_name () const { return "pub3::expr_dict_t"; }
    bool to_xdr (xpub3_expr_t *x) const;
    bool to_xdr (xpub3_json_t *x) const;

    ptr<expr_dict_t> to_dict () { return mkref (this); }
    ptr<const expr_dict_t> to_dict () const { return mkref (this); }

    bool to_bool () const { return size () > 0; }
    void replace (const str &nm, ptr<expr_t> x);

    str type_to_str () const { return "dict"; }
    ptr<mref_t> eval_to_ref (eval_t e) const;
    ptr<expr_t> deep_copy () const;
    ptr<expr_dict_t> copy_dict () const;
    bool is_static () const;
  private:
    mutable tri_bool_t _static;
  }; 

  //-----------------------------------------------------------------------

  class expr_assignment_t : public expr_t {
  public:
    expr_assignment_t (ptr<expr_t> lhs, ptr<expr_t> rhs, lineno_t l);
    expr_assignment_t (const xpub3_assignment_t &x);
    static ptr<expr_assignment_t> alloc (ptr<expr_t> l, ptr<expr_t> r);
    const char *get_obj_name () const { return "pub3::assignment_t"; }
    bool to_xdr (xpub3_expr_t *x) const;
    ptr<const expr_t> eval_to_val (eval_t e) const;
    ptr<mref_t> eval_to_ref (eval_t e) const;
    bool might_block_uncached () const;
    void pub_to_ref (publish_t pub, mrev_t ev, CLOSURE) const;
  private:
    ptr<mref_t> eval_to_ref_final (eval_t e, ptr<mref_t> lhs,
				   ptr<mref_t> rhs) const;
    ptr<expr_t> _lhs, _rhs;
    const int _lineno;
  };

  //-----------------------------------------------------------------------

};

