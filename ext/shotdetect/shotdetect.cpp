#include <ruby.h>
#include "film.h"

VALUE Narra = Qnil;
VALUE Shotdetect = Qnil;

VALUE method_detect(VALUE self, VALUE path);

extern "C" void Init_shotdetect() {
 Narra = rb_define_module("Narra");
 Shotdetect = rb_define_module_under(Narra, "Shotdetect");
 rb_define_module_function(Shotdetect, "detect", reinterpret_cast< VALUE ( * ) ( ... ) >(method_detect), 1);
}


VALUE method_detect(VALUE self, VALUE path){
 film f;
 f.set_input_file(rb_string_value_cstr(&path));
 f.process();
 
 VALUE shotlist, item;
 shotlist = rb_ary_new();
 
 std::list<shot>::const_iterator iterator;
 for (iterator = f.shots.begin(); iterator != f.shots.end(); ++iterator) {
  item = rb_hash_new();
  rb_hash_aset(item, rb_str_new2("id"), INT2FIX((*iterator).myid));
  rb_hash_aset(item, rb_str_new2("time"), rb_float_new((*iterator).msbegin/1000));
  rb_hash_aset(item, rb_str_new2("score"), INT2FIX((*iterator).score));
  rb_ary_push(shotlist, item);
 }
 
 return shotlist;
}
