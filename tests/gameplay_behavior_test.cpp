#include "../src/gameplay.cpp"
#include <cassert>
#include <map>
#include <iostream>
#include <codecvt>
#include <locale>
using namespace il2cpp;
namespace {
struct C;struct F{const char* name;C* type;uint32_t flags=0;};
struct C{const char* ns;const char* name;C* parent=nullptr;std::vector<F*> fields;std::map<std::string,const Method*> methods;C(const char* space,const char* label,C* base=nullptr):ns(space),name(label),parent(base){} };
struct N{
 C* klass;std::u16string name;N *t=nullptr,*go=nullptr,*parent=nullptr;
 std::vector<N*> children;std::map<C*,N*> components;
 std::map<F*,uint8_t> bytes;std::vector<uint8_t> values;
 std::map<F*,N*> refs;std::map<F*,bool> flags;std::map<F*,float> floats;
 int id=0;bool enabled=true,active=true;float speed=1;gameplay::Vec3 p{},destination{};gameplay::Quat q{0,0,0,1};
 N(C* c,const char* label):klass(c){for(;*label;++label)name+=char16_t(*label);}
};
Object* O(N* n){return reinterpret_cast<Object*>(n);}N* Node(void* o){return reinterpret_cast<N*>(o);}
Class* K(C* c){return reinterpret_cast<Class*>(c);}C* ClassNode(Class* c){return reinterpret_cast<C*>(c);}
Field* MakeField(F* f){return reinterpret_cast<Field*>(f);}F* FieldNode(Field* f){return reinterpret_cast<F*>(f);}
const Method* M(uintptr_t n){return reinterpret_cast<const Method*>(n);}
std::map<Handle,Object*> roots;Handle serial=0;bool master=true,yes=true;
N *local_player=nullptr,*current_room=nullptr,*fresh_targets=nullptr;
gameplay::Scene scene{7};int target_initializers=0,player_queries=0,native_updates=0;
N* victim_transform=nullptr;
bool owned=true;int spider_walks=0,spider_routes=0;gameplay::Vec3 velocity{1,0,0};
std::map<F*,int> hashes;std::map<int,bool> animation_bools;std::map<int,float> animation_floats;
N* native_points=nullptr;
C* array_class=nullptr;std::vector<std::unique_ptr<N>> result_arrays;
int sight_checks=0,mom_checks=0,door_requests=0;bool fake_sight=true;
gameplay::Vec3 ray_end{};std::map<C*,N*> find_arrays;N *ray_collider=nullptr;bool ray_hits=true,ray_error=false;
N *chat_array=nullptr,*room_players=nullptr;bool input_focus=false;std::u16string draft;
std::vector<std::u16string> notices;std::vector<std::unique_ptr<std::u16string>> strings;
String* NewString(const char* text){auto s=std::make_unique<std::u16string>(std::wstring_convert<std::codecvt_utf8_utf16<char16_t>,char16_t>{}.from_bytes(text));auto* p=s.get();strings.push_back(std::move(s));return reinterpret_cast<String*>(p);}
F* test_difficulty_field=nullptr;uint8_t test_difficulty=1;bool refresh_error=false;int difficulty_helper_calls=0;
N* assist_list=nullptr;int assist_rays=0,assisted_uses=0;uint8_t observed_hand=0;bool observed_placing=true,assist_throw=false,assist_fresh=false,prerequisite=true;
int sight_local_calls=0,sight_network_calls=0,recover_clears=0,recover_resets=0,recover_routes=0;
bool sight_error=false;N* recovery_route=nullptr;
int step_calls=0;void NativeStep(Object*,const Method*){++step_calls;}
void NativeSight(Object*,bool,bool,const Method*);
Object* NativeNoise(Object*,const Method*){return O(fresh_targets);}
std::map<F*,int> reference_writes;
Handle NewRoot(Object* o,bool){roots[++serial]=o;return serial;}Object* GetRoot(Handle h){return roots.at(h);}void FreeRoot(Handle h){assert(roots.erase(h));}
Class* ObjectClass(Object* o){return K(Node(o)->klass);}
Class* Parent(Class* c){return K(ClassNode(c)->parent);}
const char* Name(Class* c){return ClassNode(c)->name;}const char* Namespace(Class* c){return ClassNode(c)->ns;}
Field* FindField(Class* c,const char* name){for(auto* f:ClassNode(c)->fields)if(!std::strcmp(f->name,name))return MakeField(f);return nullptr;}
Field* NextField(Class* c,void** it){const size_t n=reinterpret_cast<uintptr_t>(*it);if(n>=ClassNode(c)->fields.size())return nullptr;*it=reinterpret_cast<void*>(n+1);return MakeField(ClassNode(c)->fields[n]);}
const char* FieldName(Field* f){return FieldNode(f)->name;}uint32_t FieldFlags(Field* f){return FieldNode(f)->flags;}
const Type* FieldType(Field* f){return reinterpret_cast<const Type*>(FieldNode(f)->type);}
Class* TypeClass(const Type* t){return reinterpret_cast<Class*>(const_cast<Type*>(t));}
const Type* ClassType(Class* c){return reinterpret_cast<const Type*>(c);}Object* Reflection(const Type* t){return reinterpret_cast<Object*>(const_cast<Type*>(t));}
const Method* FindMethodForTest(Class* c,const char* name,int arity){auto i=ClassNode(c)->methods.find(std::string(name)+":"+std::to_string(arity));return i==ClassNode(c)->methods.end()?nullptr:i->second;}
void Read(Object* o,Field* field,void* out){auto* f=FieldNode(field);auto* n=Node(o);if(!std::strcmp(f->type->name,"PickUpObject"))*static_cast<uint8_t*>(out)=n->bytes[f];else if(!std::strcmp(f->type->name,"Boolean"))*static_cast<bool*>(out)=n->flags[f];else if(!std::strcmp(f->type->name,"Single"))*static_cast<float*>(out)=n->floats[f];else *static_cast<Object**>(out)=O(n->refs[f]);}
void Write(Object* o,Field* field,void* p){auto* f=FieldNode(field);auto* n=Node(o);if(!std::strcmp(f->type->name,"PickUpObject"))n->bytes[f]=*static_cast<uint8_t*>(p);else if(!std::strcmp(f->type->name,"Boolean"))n->flags[f]=*static_cast<bool*>(p);else{assert(!std::strcmp(f->type->name,"Single"));n->floats[f]=*static_cast<float*>(p);}}
void ReferenceWrite(Object* o,Field* f,Object* value){Node(o)->refs[FieldNode(f)]=Node(value);++reference_writes[FieldNode(f)];}
void StaticRead(Field* f,void* out){if(FieldNode(f)==test_difficulty_field)*static_cast<uint8_t*>(out)=test_difficulty;else *static_cast<int*>(out)=hashes.at(FieldNode(f));}
void StaticWrite(Field* f,void* p){if(FieldNode(f)==test_difficulty_field){test_difficulty=*static_cast<uint8_t*>(p);return;}assert(false);}
bool Below(N* n,N* root){for(int i=0;n&&i<32;++i,n=n->parent)if(n==root)return true;return false;}
Object* Invoke(const Method* method,void* self,void** args,Object** error){
 *error=nullptr;auto* n=Node(self);static bool boolean;static int count;
 switch(reinterpret_cast<uintptr_t>(method)){
 case 1:return reinterpret_cast<Object*>(&yes); // Unity op_Implicit
 case 2:return O(n->t);case 3:return O(n->go);case 4:boolean=n->active;return reinterpret_cast<Object*>(&boolean);
 case 5:return reinterpret_cast<Object*>(&n->enabled);case 6:n->enabled=*static_cast<bool*>(args[0]);return nullptr;
 case 7:return reinterpret_cast<Object*>(&n->p);case 8:return reinterpret_cast<Object*>(&n->q);
 case 9:return O(n->parent);case 10:count=int(n->children.size());return reinterpret_cast<Object*>(&count);
 case 11:return O(n->children.at(*static_cast<int*>(args[0])));
 case 12:return reinterpret_cast<Object*>(&n->name);
 case 13:{auto it=n->components.find(reinterpret_cast<C*>(args[0]));return it==n->components.end()?nullptr:O(it->second);}
 case 14:return reinterpret_cast<Object*>(&n->speed);case 15:n->speed=*static_cast<float*>(args[0]);return nullptr;
 case 16:return O(fresh_targets); // runtime animator asset, never written
 case 17:++target_initializers;return nullptr;
 case 18:++player_queries;for(auto* f:n->klass->fields)if(!std::strcmp(f->name,"seeTargets"))n->refs[f]=fresh_targets;return nullptr;
 case 19:boolean=Below(n,Node(args[0]));return reinterpret_cast<Object*>(&boolean);
 case 20:return reinterpret_cast<Object*>(&master);case 21:return O(current_room);case 22:return reinterpret_cast<Object*>(&scene);
 case 23:return reinterpret_cast<Object*>(&yes);case 24:return O(local_player);
 case 25:n->destination=*static_cast<gameplay::Vec3*>(args[0]);return reinterpret_cast<Object*>(&yes);
 case 26:n->active=*static_cast<bool*>(args[0]);return nullptr;
 case 27:count=int(n->children.size());return reinterpret_cast<Object*>(&count);
 case 28:return O(n->children.at(*static_cast<int*>(args[0])));
 case 29:return reinterpret_cast<Object*>(&owned);
 case 30:++spider_walks;return nullptr;
 case 31:return reinterpret_cast<Object*>(&velocity);
 case 32:animation_bools[*static_cast<int*>(args[0])]=*static_cast<bool*>(args[1]);return nullptr;
 case 33:animation_floats[*static_cast<int*>(args[0])]=*static_cast<float*>(args[1]);return nullptr;
 case 34:++spider_routes;native_points->children={victim_transform};return nullptr;
 case 35:{
  auto array=std::make_unique<N>(array_class,"result");auto* wanted=reinterpret_cast<C*>(args[0]);
  const auto walk=[&](auto&& walk,N* t)->void{if(!t)return;auto i=t->components.find(wanted);if(i!=t->components.end())array->children.push_back(i->second);for(auto* c:t->children)walk(walk,c);};
  walk(walk,n->t);auto* value=array.get();result_arrays.push_back(std::move(array));return O(value);
 }
 case 36:{static int owner=1;return reinterpret_cast<Object*>(&owner);}
 case 37:return O(local_player);
 case 38:n->p=*static_cast<gameplay::Vec3*>(args[0]);return nullptr;
 case 39:assert(false);return nullptr; // no disconnect path allowed
 case 40:return reinterpret_cast<Object*>(&n->id);
 case 41:assert(false);return nullptr;
 case 43:{++sight_checks;auto* back=FindField(K(n->klass),"enemyAIGrannyMulti");if(!back)back=FindField(K(n->klass),"enemyAIGrandpaMulti");auto* owner=n->refs[FieldNode(back)];assert(owner);owner->flags[FieldNode(FindField(K(owner->klass),"seePlayer"))]=fake_sight;auto* player_field=FindField(K(n->klass),"pka");if(!player_field)player_field=FindField(K(n->klass),"pdz");n->refs[FieldNode(player_field)]=local_player;return nullptr;}
 case 44:++mom_checks;n->flags[FieldNode(FindField(K(n->klass),"isPlayerInView"))]=fake_sight;n->refs[FieldNode(FindField(K(n->klass),"rnx"))]=local_player;n->flags[FieldNode(FindField(K(n->klass),"hunting"))]=true;return nullptr;
 case 45:count=int(n->values.size());return reinterpret_cast<Object*>(&count);
 case 46:return reinterpret_cast<Object*>(&n->values.at(*static_cast<int*>(args[0])));
 case 47:{gameplay::DifficultyHelper<0,0>(O(n),nullptr);if(refresh_error){*error=O(local_player);return nullptr;}for(auto* f:n->klass->fields)if(!std::strcmp(f->type->name,"Single"))n->floats[f]=float(test_difficulty+1);return nullptr;}
 case 48:n->values.clear();return nullptr;
 case 49:{auto* f=FindField(K(n->klass),"wayPoints");auto* points=n->refs[FieldNode(f)];assert(points);points->children.push_back(local_player);if(refresh_error){*error=O(local_player);return nullptr;}n->floats[FieldNode(FindField(K(n->klass),"wanderSpeed"))]=5;return nullptr;}
 case 50:{gameplay::SightRequestHook(O(n),*static_cast<bool*>(args[0]),*static_cast<bool*>(args[1]),nullptr);if(sight_error)*error=O(local_player);return nullptr;}
 case 51:{++recover_clears;for(auto* f:n->klass->fields)if(!std::strcmp(f->name,"pka")||!std::strcmp(f->name,"pdz")||!std::strcmp(f->name,"rnx"))n->refs[f]=nullptr;return nullptr;}
 case 52:++recover_resets;return nullptr;
 case 53:{++recover_routes;n->refs[FieldNode(FindField(K(n->klass),"target"))]=recovery_route;return nullptr;}
 case 54:{++recover_clears;n->refs[FieldNode(FindField(K(n->klass),"rnx"))]=nullptr;n->flags[FieldNode(FindField(K(n->klass),"hunting"))]=false;n->refs[FieldNode(FindField(K(n->klass),"target"))]=recovery_route;return nullptr;}
 case 55:if(args){auto i=find_arrays.find(reinterpret_cast<C*>(args[0]));return O(i==find_arrays.end()?chat_array:i->second);}return O(room_players);
 case 56:{static int32_t time=1000;return reinterpret_cast<Object*>(&time);}
 case 57:return reinterpret_cast<Object*>(&draft);
 case 58:draft=*reinterpret_cast<std::u16string*>(args[0]);return nullptr;
 case 59:notices.push_back(draft);draft.clear();return nullptr;
 case 60:return reinterpret_cast<Object*>(&input_focus);
 case 61:++door_requests;return nullptr;
 case 62:ray_end=*static_cast<gameplay::Vec3*>(args[1]);assert(*static_cast<int*>(args[3])==~0&&*static_cast<int*>(args[4])==1);if(ray_error)*error=O(local_player);return reinterpret_cast<Object*>(&ray_hits);
 case 63:return O(ray_collider);
 default:assert(false);return nullptr;
 }
}
void* Unbox(Object* o){return o;}int32_t StringLength(String* s){return int32_t(reinterpret_cast<std::u16string*>(s)->size());}
void NativeSight(Object* o,bool on,bool network,const Method*){
 auto* n=Node(o);if(network)++sight_network_calls;else{++sight_local_calls;n->refs[FieldNode(FindField(K(n->klass),"grannySeeTarget"))]->active=on;}
}
void NativeSightRpc(Object* o,bool on,const Method*){NativeSight(o,on,false,nullptr);}
const uint16_t* StringChars(String* s){return reinterpret_cast<const uint16_t*>(reinterpret_cast<std::u16string*>(s)->data());}
void Connect(N& object,N& t,N& go){object.t=&t;object.go=&go;t.t=&t;t.go=&go;go.t=&t;go.go=&go;t.components[object.klass]=&object;}
Object* AllocateForTest(Class* c){auto value=std::make_unique<N>(ClassNode(c),"temporary list");auto* result=value.get();result_arrays.push_back(std::move(value));return O(result);}
Object* Requirements(Object*,const Method*){return O(assist_list);}
bool Placing(Object*,const Method*){return true;}
void NativeRay(Object* o,const Method*){
 ++assist_rays;if(!prerequisite)return;
 auto* target=gameplay::runtime->Ref(o,"rgn");if(!target)return;
 gameplay::InteractionRequirements(target,nullptr);
 auto* hand=FieldNode(gameplay::assist_hand);observed_hand=Node(o)->bytes[hand];
 observed_placing=gameplay::InteractionPlacing(target,nullptr);if(!observed_placing)++assisted_uses;
 if(assist_fresh)Node(o)->bytes[hand]=9;
 if(assist_throw)throw 7;
}
void OriginalDifficultyHelper(Object*,const Method*){++difficulty_helper_calls;}
void NativeAttack(Object* o,const Method*){
 ++native_updates;auto* n=Node(o);n->flags[FieldNode(FindField(K(n->klass),"playerGetCaught"))]=true;
 n->refs[FieldNode(FindField(K(n->klass),"targetTransform"))]=victim_transform;
}
}
extern "C" int __android_log_print(int,const char*,const char*,...){return 0;}
extern "C" int DobbyHook(void*,void*,void**){return -1;}
extern "C" int DobbyHookNear(void*,void*,void**){return -1;}
int main(){
 C unity{"UnityEngine","Object"},component{"UnityEngine","Component",&unity},transform{"UnityEngine","Transform",&component},go{"UnityEngine","GameObject",&unity};
 C behavior{"UnityEngine","Behaviour",&component},anim{"UnityEngine","Animator",&behavior},camera{"UnityEngine","Camera",&behavior},nav{"UnityEngine.AI","NavMeshAgent",&behavior};
 C ai{"","EnemyAIGrannyMulti",&behavior},eye{"","EnemyEye",&behavior},movement{"","FirstPersonController",&behavior},controller{"UnityEngine","CharacterController",&component};
 C integer{"System","Int32"};
 C player{"Photon.Realtime","Player"},view{"Photon.Pun","PhotonView",&behavior},list{"System.Collections.Generic","List`1"},single{"System","Single"},boolean{"System","Boolean"};
 F pv{"pv",&view},anim_field{"grannyAIAnim",&anim},eye_field{"enemyEye",&eye},door{"doorRay",&go},manager{"gameManager",&go},target{"target",&transform},target_transform{"targetTransform",&transform},pos{"playerPos",&transform},route{"nav1",&transform},agent{"navComponent",&nav},speed{"grannysVarSpeed",&single};
 F see_player{"seePlayer",&boolean};
 F caught{"playerGetCaught",&boolean},attacking{"attackingPlayer",&boolean},walk{"startWalk",&boolean},hit{"hitByGun",&boolean};
 ai.fields={&pv,&anim_field,&eye_field,&door,&manager,&target,&target_transform,&pos,&route,&agent,&speed,&caught,&attacking,&walk,&hit,&see_player};ai.methods["ekr:0"]=M(17);
 F sight_range{"seeRange",&single},sight_rate{"grannyUpdateParseRate",&single},days{"daysManager",&go};
 F back{"enemyAIGrannyMulti",&ai},cam{"cam",&camera},my_transform{"myTransform",&transform},targets{"seeTargets",&list},selected{"pka",&player};eye.fields={&pv,&back,&cam,&my_transform,&targets,&selected,&sight_range,&sight_rate,&days};eye.methods["GetPlayers:0"]=M(18);
 F body_field{"controller",&controller};movement.fields={&body_field};
 N source(&ai,"source"),copy(&ai,"copy"),st(&transform,"root"),dt(&transform,"root"),sg(&go,"root"),dg(&go,"root");Connect(source,st,sg);Connect(copy,dt,dg);
 N se(&eye,"eye"),de(&eye,"eye"),et(&transform,"eye"),eg(&go,"eye"),det(&transform,"eye"),deg(&go,"eye");Connect(se,et,eg);Connect(de,det,deg);st.children={&et};dt.children={&det};et.parent=&st;det.parent=&dt;
 N sc(&camera,"cam"),dc(&camera,"cam");Connect(sc,et,eg);Connect(dc,det,deg);
 N sa(&anim,"anim"),da(&anim,"anim");Connect(sa,st,sg);Connect(da,dt,dg);da.enabled=false;da.speed=0;
 N sn(&nav,"nav"),dn(&nav,"nav");Connect(sn,st,sg);Connect(dn,dt,dg);
 N scene_t(&transform,"scene"),scene_go(&go,"scene"),door_go(&go,"door"),door_t(&transform,"door");Connect(scene_go,scene_t,scene_go);Connect(door_go,door_t,door_go);
 N source_view(&view,"old view"),copy_view(&view,"new view"),old_list(&list,"old targets"),new_list(&list,"new targets"),asset(&unity,"controller");fresh_targets=&new_list;
 source.refs={{&pv,&source_view},{&anim_field,&sa},{&eye_field,&se},{&door,&door_go},{&manager,&scene_go},{&target,&scene_t},{&route,&scene_t},{&agent,&sn}};
 copy.refs={{&pv,&copy_view},{&anim_field,&da},{&eye_field,&de},{&agent,&dn}};
 se.floats[&sight_range]=30;se.floats[&sight_rate]=.25f;
 se.refs={{&days,&scene_go},{&back,&source},{&cam,&sc},{&my_transform,&et},{&targets,&old_list}};de.refs={{&back,&source},{&my_transform,&det}};
 Api api{};api.gchandle_new=NewRoot;api.gchandle_get_target=GetRoot;api.gchandle_free=FreeRoot;api.runtime_invoke=Invoke;api.object_unbox=Unbox;
 api.object_get_class=ObjectClass;api.class_get_parent=Parent;api.class_get_name=Name;api.class_get_namespace=Namespace;api.class_get_field_from_name=FindField;api.class_get_fields=NextField;api.field_get_name=FieldName;api.field_get_flags=FieldFlags;
 api.field_get_type=FieldType;api.class_from_type=TypeClass;api.class_get_type=ClassType;api.type_get_object=Reflection;api.class_get_method_from_name=FindMethodForTest;api.field_get_value=Read;api.field_set_value=Write;api.field_set_value_object=ReferenceWrite;api.string_length=StringLength;api.string_chars=StringChars;
 {
 gameplay::Runtime r(api);r.alive=M(1);r.transform=r.go_transform=M(2);r.go=M(3);r.go_active=M(4);r.enabled_get=M(5);r.enabled_set=M(6);r.pos_get=M(7);r.rot_get=M(8);r.parent_get=M(9);r.child_count=M(10);r.child_get=M(11);r.name_get=M(12);r.get_component=M(13);r.anim_get=M(14);r.anim_set=M(15);r.anim_controller_get=M(16);r.is_child=M(19);r.active_set=M(26);
 r.actors.push_back(std::make_unique<gameplay::Actor>(api,O(&source),0));
 assert(r.PrepareCloneBehavior(O(&copy),0));
 assert(copy.refs[&pv]==&copy_view&&source.refs[&pv]==&source_view); // network identity preserved
 assert(copy.refs[&door]==&door_go&&copy.refs[&manager]==&scene_go);
 assert(de.refs[&back]==&copy&&de.refs[&cam]==&dc&&se.refs[&back]==&source&&se.refs[&cam]==&sc);
 assert(de.refs[&targets]==fresh_targets&&de.refs[&targets]!=se.refs[&targets]);
 assert(da.enabled&&da.speed==1&&sa.speed==1&&target_initializers==1&&player_queries==1);
 assert(!copy.refs[&target]&&!copy.refs[&pos]); // dynamic targets aren't copied from the original
 // A hierarchy mismatch must not attach the original enemy's camera to a clone.
 de.refs[&cam]=nullptr;det.name=u"wrong child";assert(!r.PrepareCloneBehavior(O(&copy),0)&&!de.refs[&cam]);det.name=u"eye";
 assert(r.PrepareCloneBehavior(O(&copy),0));
 // Missing optional source reflection cannot reject healthy local bindings.
 auto fields=api.class_get_fields;api.class_get_fields=nullptr;assert(r.PrepareCloneBehavior(O(&copy),0));api.class_get_fields=fields;
 // Match the device log: every original actor was removed before cloning.
 array_class=&list;r.children=M(35);r.length=M(27);r.item=M(28);auto original_actors=std::move(r.actors);
 r.clones.push_back(std::make_unique<gameplay::CloneRecord>(api,O(&dg),O(&copy_view),gameplay::Vec3{},0));r.clones.back()->controller.Set(O(&copy));
 copy.refs[&pv]=&source_view;de.refs[&pv]=&source_view;de.refs[&back]=&source;de.refs[&cam]=nullptr;de.refs[&my_transform]=&et;
 assert(r.PrepareCloneBehavior(O(&copy),0));
 assert(copy.refs[&pv]==&copy_view&&de.refs[&pv]==&copy_view&&de.refs[&back]==&copy&&de.refs[&cam]==&dc&&de.refs[&my_transform]==&det);
 // A missing DaysManager comes from its unique typed scene component, not
 // another enemy. A missing/ambiguous scene component is rejected.
 C days_class{"","DaysManager",&behavior};N days_component(&days_class,"days"),days_nodes(&list,"days list");Connect(days_component,scene_t,scene_go);find_arrays[&days_class]=&days_nodes;r.find=M(55);
 auto* prior_days_type=days.type;days.type=&days_class;de.refs[&days]=nullptr;days_nodes.children={&days_component};
 assert(r.PrepareCloneBehavior(O(&copy),0)&&de.refs[&days]==&days_component);
 de.refs[&days]=nullptr;days_nodes.children.clear();assert(!r.PrepareCloneBehavior(O(&copy),0));days.type=prior_days_type;de.refs[&days]=&scene_go;
 r.clones.clear();r.actors=std::move(original_actors);
 // A protected victim must release only its own attacker, without disabling AI.
 N local(&player,"local"),other(&player,"other"),room(&unity,"room"),body_t(&transform,"player"),body_go(&go,"player"),body(&controller,"controller"),move(&movement,"movement");
 Connect(body,body_t,body_go);move.refs[&body_field]=&body;local_player=&local;current_room=&room;r.movement.Set(O(&move));
 r.master=M(20);r.current_room=M(21);r.nav_set=M(15);r.nav_destination=M(25);r.snap.ready=r.snap.authority=true;
 gameplay::hook_api=&api;gameplay::unity_thread=std::this_thread::get_id();gameplay::scene_method=M(22);gameplay::scene_valid_method=gameplay::scene_loaded_method=M(23);gameplay::room_method=M(21);gameplay::protected_room=reinterpret_cast<uintptr_t>(O(&room));gameplay::protected_scene=7;gameplay::local_player_method=M(24);
 r.actors.push_back(std::make_unique<gameplay::Actor>(api,O(&copy),0));auto& actor=*r.actors.back();
 scene_t.p={10,0,10};copy.refs[&route]=&scene_t;copy.refs[&target]=copy.refs[&target_transform]=&body_t;de.refs[&selected]=&local;copy.flags[&caught]=copy.flags[&attacking]=copy.flags[&walk]=true;copy.floats[&speed]=4;dn.speed=0;
 r.CombatGuard(actor,true);assert(copy.flags[&caught]&&copy.flags[&attacking]&&dn.speed==0);actor.attack_release=gameplay_bridge::Now()-1;
 r.CombatGuard(actor,true);assert(!copy.flags[&caught]&&!copy.flags[&attacking]&&!copy.flags[&walk]&&copy.refs[&target]==&body_t&&dn.speed==4&&copy.enabled);
 assert(copy.refs[&target_transform]==&body_t&&de.refs[&selected]==&local);
 copy.flags[&caught]=true;copy.refs[&target_transform]=&body_t;de.refs[&selected]=&other;dn.speed=0;r.CombatGuard(actor,true);assert(copy.flags[&caught]&&dn.speed==0); // another selected player overrides stale own transform
 de.refs[&selected]=&local;r.CombatGuard(actor,false);assert(copy.flags[&caught]);
 master=false;r.CombatGuard(actor,true);assert(copy.flags[&caught]);master=true;
 copy.flags[&hit]=true;r.CombatGuard(actor,true);assert(copy.flags[&caught]);copy.flags[&hit]=false;
 r.CombatGuard(actor,true);assert(copy.flags[&caught]);actor.attack_release=gameplay_bridge::Now()-1;r.CombatGuard(actor,true);assert(!copy.flags[&caught]&&dn.speed==4);
  // An attack state entered inside the original update is released afterwards.
  gameplay::runtime=&r;r.snap.god=r.snap.god_ready=true;victim_transform=&body_t;
  gameplay::original_enemy_update[0]=NativeAttack;
  gameplay::EnemyUpdate<0>(O(&copy),nullptr);assert(native_updates==1&&copy.flags[&caught]&&actor.attack_release>gameplay_bridge::Now()&&copy.refs[&target]==&body_t);
  gameplay::runtime=nullptr;
  F grandpa_selected{"pdz",&player};eye.fields.push_back(&grandpa_selected);actor.kind=1;
  de.refs[&grandpa_selected]=&other;copy.refs[&target_transform]=&body_t;copy.flags[&caught]=true;r.CombatGuard(actor,true);assert(copy.flags[&caught]);
  de.refs[&grandpa_selected]=&local;r.CombatGuard(actor,true);assert(copy.flags[&caught]);actor.attack_release=gameplay_bridge::Now()-1;r.CombatGuard(actor,true);assert(!copy.flags[&caught]);actor.kind=0;
  // MomSpider's rejected grab must release eating/hunting too, without a death revival.
  C mom_class{"","SpiderMomAIController",&behavior};
  F mom_owner{"rnx",&player},mom_target{"target",&transform},points{"wayPoints",&list},mom_nav{"agent",&nav},wander{"wanderSpeed",&single};
  F eating{"eatingPlayer",&boolean},hunting{"hunting",&boolean},in_view{"isPlayerInView",&boolean},weapon_hit{"gotHitByWeapon",&boolean},mom_pos{"rnz",&transform},mom_go{"rny",&go};
  mom_class.fields={&mom_owner,&mom_target,&points,&mom_nav,&wander,&eating,&hunting,&in_view,&weapon_hit,&mom_pos,&mom_go};
  list.methods["get_Count:0"]=M(27);list.methods["get_Item:1"]=M(28);
  N mom(&mom_class,"mom"),waypoints(&list,"points"),mom_agent(&nav,"agent");waypoints.children={&scene_t};
  mom.refs={{&mom_owner,&local},{&mom_target,&body_t},{&points,&waypoints},{&mom_nav,&mom_agent}};mom.flags[&eating]=mom.flags[&hunting]=mom.flags[&in_view]=true;mom.floats[&wander]=3;
  gameplay::Actor mom_actor(api,O(&mom),2);r.CombatGuard(mom_actor,true);assert(mom.flags[&eating]);mom_actor.attack_release=gameplay_bridge::Now()-1;r.CombatGuard(mom_actor,true);
  assert(!mom.flags[&eating]&&mom.flags[&hunting]&&mom.flags[&in_view]&&mom.refs[&mom_target]==&body_t&&mom.refs[&mom_owner]==&local&&mom_agent.speed==3);
  mom.refs[&mom_owner]=&other;mom.flags[&eating]=true;r.CombatGuard(mom_actor,true);assert(mom.flags[&eating]);
  mom.refs[&mom_owner]=&local;mom.flags[&weapon_hit]=true;r.CombatGuard(mom_actor,true);assert(mom.flags[&eating]);
  // A client driver acts only on this fresh owned clone, preserving original
  // scene agents and the real master identity. Its native route list is local.
  N mt(&transform,"mom"),mg(&go,"mom");Connect(mom,mt,mg);
  F animator_field{"momAnimator",&anim};mom_class.fields.push_back(&animator_field);mom.refs[&animator_field]=&da;
  mom.flags[&weapon_hit]=mom.flags[&eating]=false;mom_agent.enabled=false;
  mom_class.methods["imz:0"]=M(30);mom_class.methods["imw:0"]=M(34);
  r.mine=M(29);master=false;waypoints.children.clear();native_points=&waypoints;victim_transform=&scene_t;
  gameplay::CloneRecord spider(api,O(&mg),O(&copy_view),{},gameplay_bridge::Now());spider.controller.Set(O(&mom));spider.kind=2;
  assert(r.DriveClientClone(spider,10)&&mom_agent.enabled&&spider_walks==1&&spider_routes==1&&gameplay::Near(mom_agent.destination,scene_t.p));
  assert(waypoints.children.size()==1&&spider.route_next>10);
  r.settings[2].paused=true;assert(r.DriveClientClone(spider,11)&&spider_walks==1);r.settings[2].paused=false;
  owned=false;assert(!r.DriveClientClone(spider,12));r.StopClientPatrol(spider);assert(!mom_agent.enabled&&spider_walks==1);owned=true;
  // Per-clone settings do not leak into the source enemy or another clone.
  r.clones.push_back(std::make_unique<gameplay::CloneRecord>(api,O(&dg),O(&copy_view),gameplay::Vec3{},gameplay_bridge::Now()));
  auto& configured=*r.clones.back();configured.controller.Set(O(&copy));configured.custom_settings=true;configured.settings.speed=2;configured.settings.size=1.5f;
  assert(r.EffectiveSettings(actor).speed==2&&r.EffectiveSettings(actor).size==1.5f&&r.EffectiveSettings(*r.actors.front()).speed==1);
  // Locomotion uses the game's actual typed static animator hashes. Attack
  // flags take precedence so repair cannot overwrite an attack animation.
  F idle_hash{"pjr",&integer,0x30},walk_hash{"pjs",&integer,0x30},speed_hash{"pjt",&integer,0x30};ai.fields.insert(ai.fields.end(),{&idle_hash,&walk_hash,&speed_hash});
  hashes={{&idle_hash,101},{&walk_hash,102},{&speed_hash,103}};api.field_static_get_value=StaticRead;
  r.nav_velocity=M(31);r.anim_bool=M(32);r.anim_float=M(33);master=true;configured.behavior_ready=configured.route_initialized=true;
  copy.flags[&caught]=copy.flags[&attacking]=copy.flags[&hit]=false;r.AnimateClones();
  assert(animation_bools[102]&&!animation_bools[101]&&animation_floats[103]==1);
  animation_bools.clear();copy.flags[&attacking]=true;r.AnimateClones();assert(animation_bools.empty());copy.flags[&attacking]=false;
  owned=false;r.AnimateClones();assert(animation_bools.empty());owned=true;
  master=false;r.AnimateClones();assert(animation_bools.empty());
  // Human client clones need their own agent/route driver: the real native
  // AI remains master-only. An authenticated owned clone can patrol, whereas
  // originals, lost ownership and paused scenes cannot be driven by it.
  configured.client_patrol=true;configured.client_nav_enabled=true;
  dn.enabled=false;configured.route_initialized=true;configured.waypoint=0;configured.route_next=0;
  assert(r.DriveClientClone(configured,20)&&dn.enabled&&gameplay::Near(dn.destination,scene_t.p));
  r.AnimateClones();assert(animation_bools[102]);animation_bools.clear();
  r.settings[0].paused=true;const auto next_route=configured.route_next;
  assert(r.DriveClientClone(configured,30)&&configured.route_next==next_route);r.AnimateClones();assert(animation_bools.empty());r.settings[0].paused=false;
  owned=false;assert(!r.DriveClientClone(configured,31));r.StopClientPatrol(configured);assert(!dn.enabled&&!configured.client_patrol);owned=true;
  F grandpa_animator{"grandpaAIAnim",&anim},grandpa_idle{"pdp",&integer,0x30},grandpa_walk{"pdq",&integer,0x30},grandpa_speed{"pdr",&integer,0x30};
  ai.fields.insert(ai.fields.end(),{&grandpa_animator,&grandpa_idle,&grandpa_walk,&grandpa_speed});copy.refs[&grandpa_animator]=&da;
  hashes[&grandpa_idle]=201;hashes[&grandpa_walk]=202;hashes[&grandpa_speed]=203;
  configured.kind=1;configured.client_patrol=true;configured.route_next=0;configured.waypoint=0;
  assert(r.DriveClientClone(configured,40)&&dn.enabled&&sn.speed==1);r.AnimateClones();assert(animation_bools[202]&&!animation_bools[201]);
  r.StopClientPatrol(configured);assert(!dn.enabled);
  master=true;
  // Missing optional source reflection cannot block a healthy clone's own
  // serialized camera, scene bindings, agent or animator.
  r.clones.clear();r.classes[0]=K(&ai);r.nav_class=K(&nav);r.animator_class=K(&anim);r.view_class=K(&view);
  array_class=&list;r.children=M(35);r.length=M(27);r.item=M(28);r.go_transform=M(2);r.pos_set=M(38);r.nav_get=M(14);
  r.master_client=M(37);r.actor_number=r.owner_number=M(36);r.nav_destination=M(25);r.nav_set=M(15);
  r.clones.push_back(std::make_unique<gameplay::CloneRecord>(api,O(&dg),O(&copy_view),gameplay::Vec3{1,0,1},gameplay_bridge::Now()-1));
  r.clones.back()->controller.Set(O(&copy));
  auto enumerator=api.class_get_fields;api.class_get_fields=nullptr;dn.enabled=false;da.enabled=false;
  r.UpdateClones();assert(r.clones.back()->route_initialized&&r.clones.back()->behavior_ready&&dn.enabled&&da.enabled);api.class_get_fields=enumerator;
  // Lists alone aren't sight. The host must call this clone's actual
  // native perception method, on a bounded schedule, to acquire a target.
  eye.methods["CheckForPlayersInView:0"]=M(43);
  copy.flags[&caught]=copy.flags[&attacking]=copy.flags[&hit]=copy.flags[&see_player]=false;
  auto& sensed=*r.clones.back();sensed.kind=0;sensed.sense_next=sensed.players_next=0;sensed.behavior_ready=false;
  assert(r.SenseClone(sensed,100)&&sight_checks==1&&sensed.has_target&&sensed.senses==1);
  assert(de.refs[&pv]==&copy_view&&de.floats[&sight_range]==30&&de.floats[&sight_rate]==.25f&&!source.flags[&see_player]);
  assert(!r.SenseClone(sensed,100.1)&&sight_checks==1);
  master=false;assert(!r.SenseClone(sensed,101)&&sight_checks==1);master=true;
  owned=false;assert(!r.SenseClone(sensed,101)&&sight_checks==1);owned=true;
  r.settings[0].paused=true;assert(!r.SenseClone(sensed,101)&&sight_checks==1);r.settings[0].paused=false;
  N* previous=current_room;current_room=&door_go;assert(!r.SenseClone(sensed,101)&&sight_checks==1);current_room=previous;
  C grandpa_class{"","EnemyAIGrandpaMulti",&behavior},grandpa_eye_class{"","EnemyEyeGrandpa",&behavior};
  F g_eye_field{"enemyEye",&grandpa_eye_class},g_see{"seePlayer",&boolean},g_back{"enemyAIGrandpaMulti",&grandpa_class},g_player{"pdz",&player};
  grandpa_class.fields={&g_eye_field,&g_see,&pv};grandpa_eye_class.fields={&g_back,&g_player,&pv,&cam,&my_transform,&sight_range,&targets};grandpa_eye_class.methods["GetPlayers:0"]=M(18);grandpa_eye_class.methods["CheckForPlayersInView:0"]=M(43);
  N g_ai(&grandpa_class,"Grandpa"),g_root(&transform,"Grandpa"),g_go(&go,"Grandpa"),g_eye(&grandpa_eye_class,"eye"),g_eye_t(&transform,"eye"),g_eye_go(&go,"eye");Connect(g_ai,g_root,g_go);Connect(g_eye,g_eye_t,g_eye_go);g_root.children={&g_eye_t};g_eye_t.parent=&g_root;
  N g_camera(&camera,"cam");Connect(g_camera,g_eye_t,g_eye_go);g_ai.refs[&pv]=&copy_view;g_ai.refs[&g_eye_field]=&g_eye;g_eye.refs={{&g_back,&g_ai},{&pv,&copy_view},{&cam,&g_camera},{&my_transform,&g_eye_t},{&days,&scene_go}};g_eye.floats[&sight_range]=30;
  gameplay::CloneRecord grandpa_sense(api,O(&g_go),O(&copy_view),{},gameplay_bridge::Now());grandpa_sense.kind=1;grandpa_sense.controller.Set(O(&g_ai));grandpa_sense.behavior_ready=true;
  F g_anim_field{"grandpaAIAnim",&anim};grandpa_class.fields.push_back(&g_anim_field);grandpa_class.methods["dwi:0"]=M(17);N g_anim(&anim,"Grandpa animator");Connect(g_anim,g_root,g_go);g_ai.refs[&g_anim_field]=&g_anim;assert(r.PrepareCloneBehavior(O(&g_ai),1));
  assert(r.SenseClone(grandpa_sense,102)&&sight_checks==2&&grandpa_sense.has_target&&g_ai.flags[&g_see]);
  F mom_camera_field{"cam",&camera},mom_visible{"isPlayerInView",&boolean};mom_class.fields.push_back(&mom_camera_field);mom_class.fields.push_back(&mom_visible);mom_class.methods["ims:0"]=M(17);mom_class.methods["inc:0"]=M(44);
  N mom_camera(&camera,"cam");Connect(mom_camera,mt,mg);mom_class.fields.push_back(&pv);mom.refs[&pv]=&copy_view;mom.refs[&mom_camera_field]=&mom_camera;mom.flags[&eating]=mom.flags[&weapon_hit]=false;
  gameplay::CloneRecord mom_sense(api,O(&mg),O(&copy_view),{},gameplay_bridge::Now());mom_sense.kind=2;mom_sense.controller.Set(O(&mom));
  N mom_anim(&anim,"Mom animator");Connect(mom_anim,mt,mg);mom.refs[&animator_field]=&mom_anim;assert(r.PrepareCloneBehavior(O(&mom),2));
  assert(r.SenseClone(mom_sense,102)&&mom_checks==1&&mom_sense.has_target);
  // A sustained loss uses native per-instance clear/reset/route; no global Start.
  eye.methods["eli:0"]=M(51);ai.methods["eim:0"]=M(52);ai.methods["ein:0"]=M(53);recovery_route=&scene_t;
  fake_sight=false;sensed.last_seen=102;sensed.has_target=true;
  assert(r.SenseClone(sensed,103)&&sensed.has_target);const int before_route=recover_routes;
  assert(r.SenseClone(sensed,104.1)&&!sensed.has_target&&recover_routes==before_route+1&&copy.refs[&target]==&scene_t);
  fake_sight=true;
  // Stationary healthy clones recover through native patrol. Category
  // pause, real weapon-hit states, ownership/host loss do not revive enemies.
  copy.flags[&caught]=copy.flags[&attacking]=copy.flags[&hit]=false;dt.p={0,0,0};
  sensed.health_next=0;sensed.health_observed=false;
  r.RecoverCloneMovement(sensed,120);r.RecoverCloneMovement(sensed,133);assert(sensed.recoveries==1);
  r.settings[0].paused=true;r.RecoverCloneMovement(sensed,150);assert(sensed.recoveries==1);r.settings[0].paused=false;
  copy.flags[&hit]=true;r.RecoverCloneMovement(sensed,170);assert(sensed.recoveries==1);copy.flags[&hit]=false;
  copy.flags[&caught]=true;r.RecoverCloneMovement(sensed,180);r.RecoverCloneMovement(sensed,196);assert(sensed.recoveries==2&&!copy.flags[&caught]);
  // A late scene dependency can recover after the former 12-try cutoff.
  gameplay::CloneRecord late(api,O(&dg),O(&copy_view),{},0);late.controller.Set(O(&copy));late.bind_tries=12;
  assert(r.SenseClone(late,199)&&late.behavior_ready&&late.bind_tries==13);
  // Only the first closed, unlocked door is interacted with. Cooldown is shared.
  C door_class{"","Door",&behavior};F open{"doorOpen",&boolean},moving{"doorMoving",&boolean},locked{"doorLocked",&boolean};door_class.fields={&open,&moving,&locked};
  N closed_door(&door_class,"door");r.door_class=K(&door_class);r.door_open=M(61);
  assert(r.OpenCloneDoor(sensed,O(&closed_door),200)&&door_requests==1);
  assert(!r.OpenCloneDoor(grandpa_sense,O(&closed_door),200.5)&&door_requests==1);
  closed_door.flags[&locked]=true;assert(!r.OpenCloneDoor(mom_sense,O(&closed_door),203)&&door_requests==1);closed_door.flags[&locked]=false;
  assert(r.OpenCloneDoor(mom_sense,O(&closed_door),203)&&door_requests==2);
  closed_door.flags[&moving]=true;assert(!r.OpenCloneDoor(sensed,O(&closed_door),206));closed_door.flags[&moving]=false;
  r.settings[0].paused=true;assert(!r.OpenCloneDoor(sensed,O(&closed_door),206));r.settings[0].paused=false;
  // Raycast stops at the first obstruction; an unrelated wall never opens a door.
  r.door_line=M(62);r.hit_collider=M(63);r.hit_size=64;r.doors.clear();
  N door_component(&door_class,"Door"),door_root(&transform,"Door"),door_object(&go,"Door"),collider(&component,"Door collider"),collider_t(&transform,"Collider"),collider_go(&go,"Collider");Connect(door_component,door_root,door_object);Connect(collider,collider_t,collider_go);collider_t.parent=&door_root;
  ray_collider=&collider;copy.refs[&target]=&scene_t;dt.p={0,0,0};scene_t.p={10,0,10};
  r.CloneDoorAhead(sensed,207);assert(door_requests==3);
  r.doors.clear();collider_t.parent=nullptr;r.CloneDoorAhead(sensed,208);assert(door_requests==3);
  sensed.investigating=true;sensed.has_target=false;sensed.noise_position={0,0,8};r.CloneDoorAhead(sensed,208.5);assert(gameplay::Near(ray_end.x,0)&&gameplay::Near(ray_end.z,1.8f));sensed.investigating=false;
  collider_t.parent=&door_root;ray_hits=false;r.CloneDoorAhead(sensed,209);assert(door_requests==3);ray_hits=true;
  ray_error=true;r.CloneDoorAhead(sensed,210);assert(door_requests==3);ray_error=false;r.door_line=nullptr;
  // Detect newly active native noise objects once; startup objects and a
  // stationary noise do not trigger repeated investigations.
  C noise_class{"","NoiseObjectFallsMulti",&behavior};N sounds(&list,"sounds"),noise(&noise_class,"noise"),noise_t(&transform,"noise"),noise_go(&go,"noise");Connect(noise,noise_t,noise_go);noise_t.p={8,0,3};
  find_arrays[&noise_class]=&sounds;r.find=M(55);r.noise_classes={K(&noise_class),nullptr};r.ScanNoise(210);assert(r.noise_serial==0);
  sounds.children={&noise};r.ScanNoise(210.3);assert(r.noise_serial==1&&r.noise_position.x==8);
  r.ScanNoise(210.6);assert(r.noise_serial==1);sounds.children.clear();r.ScanNoise(211);sounds.children={&noise};r.ScanNoise(211.5);assert(r.noise_serial==2);
  // Native sound callback is observed even if the noise was already seen by
  // the poller; it retains original IEnumerator return and deduplicates pairs.
  gameplay::runtime=&r;gameplay::original_noise[0]=NativeNoise;
  assert(gameplay::NoiseEmitted<0>(O(&noise),nullptr)==O(fresh_targets));const unsigned emitted=r.noise_serial;
  assert(gameplay::NoiseEmitted<0>(O(&noise),nullptr)==O(fresh_targets)&&r.noise_serial==emitted);
  gameplay::runtime=nullptr;
  // Player footsteps are real animation/audio events with a shorter radius.
  C steps_class{"","FootStepsController",&behavior};steps_class.fields={&pv};N steps(&steps_class,"steps");Connect(steps,noise_t,noise_go);steps.refs[&pv]=&copy_view;
  gameplay::runtime=&r;gameplay::original_footstep=NativeStep;r.noise_time=0;
  gameplay::FootstepEmitted(O(&steps),nullptr);assert(step_calls==1&&r.noise_radius==8);double step_time=r.noise_time;
  gameplay::CloneRecord listener(api,O(&dg),O(&copy_view),{},0);listener.controller.Set(O(&copy));copy.flags[&see_player]=false;dt.p={20,0,3};
  r.UpdateCloneEnvironment(listener,step_time);assert(!listener.investigating); // 12m is outside footstep radius
  dt.p={4,0,3};r.CaptureFootstep(O(&steps),step_time+1);r.UpdateCloneEnvironment(listener,step_time+1);assert(listener.investigating);
  const auto weak=r.noise_serial;r.CaptureNoise(O(&steps),step_time+1.1,20);assert(r.noise_radius==20&&r.noise_serial==weak+1); // louder drop supersedes a nearby footstep
  gameplay::runtime=nullptr;
  // Sound navigation preserves shared target transforms, yields to sight and
  // ends after reaching the sound (or timeout) through native patrol cleanup.
  sensed.has_target=false;copy.flags[&see_player]=false;copy.refs[&target]=&scene_t;
  r.noise_serial=1;r.noise_time=210;r.noise_radius=20;r.noise_position={8,0,3};dt.p={0,0,0};r.UpdateCloneEnvironment(sensed,210);
  assert(sensed.investigating&&dn.destination.x==8&&copy.refs[&target]==&scene_t&&scene_t.p.x==10);
  copy.flags[&see_player]=true;r.DriveCloneInvestigation(sensed,211);assert(!sensed.investigating);copy.flags[&see_player]=false;
  r.noise_serial=2;r.noise_time=212;r.UpdateCloneEnvironment(sensed,212);assert(sensed.investigating);
  dt.p=r.noise_position;r.DriveCloneInvestigation(sensed,213);assert(sensed.arrived_at==213);
  r.DriveCloneInvestigation(sensed,215.1);assert(!sensed.investigating);
  master=false;r.noise_serial=3;r.noise_time=216;r.UpdateCloneEnvironment(sensed,216);assert(!sensed.investigating);master=true;

 }
 {
  gameplay::Runtime r(api);r.alive=M(1);r.go=M(3);r.go_active=M(4);r.mine=M(29);r.snap.ready=true;
  C speed_class{"","FirstPersonController",&behavior};F walk_speed{"walkSpeed",&single},crouch_speed{"crouchSpeed",&single};speed_class.fields={&pv,&walk_speed,&crouch_speed};
  N self(&speed_class,"movement"),st(&transform,"movement"),sg(&go,"movement"),v(&view,"view"),room(&unity,"room");Connect(self,st,sg);self.refs[&pv]=&v;self.floats[&walk_speed]=4;self.floats[&crouch_speed]=2;r.movement.Set(O(&self));
  current_room=&room;gameplay::protected_room=reinterpret_cast<uintptr_t>(O(&room));gameplay::protected_scene=scene.handle;owned=true;
  r.snap.player_speed=3;r.UpdatePlayerSpeed();r.UpdatePlayerSpeed();assert(r.snap.player_speed_ready&&self.floats[&walk_speed]==12&&self.floats[&crouch_speed]==6);
  r.snap.player_speed=2;r.UpdatePlayerSpeed();assert(self.floats[&walk_speed]==8); // no compounding
  r.snap.player_speed=1;r.UpdatePlayerSpeed();assert(self.floats[&walk_speed]==4&&self.floats[&crouch_speed]==2);
  r.snap.player_speed=2;r.UpdatePlayerSpeed();self.floats[&walk_speed]=5;r.UpdatePlayerSpeed();assert(self.floats[&walk_speed]==10); // honor fresh game base
  r.ReleasePlayerSpeed(true);assert(self.floats[&walk_speed]==5&&self.floats[&crouch_speed]==2);
  owned=false;r.snap.player_speed=5;r.UpdatePlayerSpeed();assert(!r.snap.player_speed_ready&&self.floats[&walk_speed]==5);owned=true;
 }

 {
  gameplay::Runtime r(api);r.alive=M(1);r.go=M(3);r.go_active=M(4);r.current_room=M(21);r.master=M(20);
  C chat_class{"","ChatManager",&behavior},input_class{"TMPro","TMP_InputField",&behavior};
  F input_field{"inputField",&input_class},allowed_field{"canSendMessage",&boolean};chat_class.fields={&input_field,&allowed_field};
  N chat(&chat_class,"chat"),ct(&transform,"chat"),cg(&go,"chat"),input(&input_class,"input"),chat_list(&list,"chat list"),players(&list,"players"),room(&unity,"room"),local(&player,"Client"),victim(&player,"Target");Connect(chat,ct,cg);
  chat.refs[&input_field]=&input;chat.flags[&allowed_field]=true;chat_list.children={&chat};players.children={&local,&victim};local.id=1;victim.id=2;room_players=&players;chat_array=&chat_list;current_room=&room;local_player=&local;
  r.room_id=reinterpret_cast<uintptr_t>(O(&room));r.snap.ready=true;r.snap.authority=false;master=false;
  r.find=M(55);r.length=M(27);r.item=M(28);r.actor_number=M(40);r.vote_players=M(55);r.vote_local=M(24);r.vote_nick=M(12);r.vote_time=M(56);r.vote_send=M(59);r.vote_text=M(57);r.vote_set=M(58);r.vote_focused=M(60);
  r.vote_input_field=MakeField(&input_field);r.vote_allowed_field=MakeField(&allowed_field);r.vote_chat_class=K(&chat_class);api.string_new=NewString;
  gameplay::vote_receive_ready=true;gameplay::vote_decode=M(1);
  const double now=gameplay_bridge::Now();r.StartVote(2,1,now);assert(r.ballot.Active()&&r.snap.vote.ready&&!master);
  draft=u"draft";r.UpdateVote(now);assert(notices.empty()&&draft==u"draft");draft.clear();input_focus=true;r.UpdateVote(now+1);assert(notices.empty());input_focus=false;
  r.UpdateVote(now+2);assert(notices.size()==1&&r.ballot.phase==room_vote::Phase::Voting);
  r.UpdateVote(now+3.01);assert(notices.size()==2&&r.ballot.phase==room_vote::Phase::PrankDelay&&notices.back().find(u"atılıyor")!=std::u16string::npos);
  r.UpdateVote(now+3.5);assert(notices.size()==2);chat.flags[&allowed_field]=false;
  r.UpdateVote(now+4.1);assert(notices.size()==2&&r.ballot.Active());chat.flags[&allowed_field]=true;
  r.UpdateVote(now+5);assert(notices.size()==3&&!r.ballot.Active()&&notices.back()==u"Şaka şaka, Target adlı kişiyi atmıyoruz ama adam olsun.");
  // A tie sends only the result; no prank reveal. Leaving the room cancels.
  notices.clear();r.StartVote(2,1,now+6);r.UpdateVote(now+6);r.ballot.no=1;r.UpdateVote(now+7.1);assert(notices.size()==2&&!r.ballot.Active()&&notices.back().find(u"eşitlik")!=std::u16string::npos);
  r.StartVote(2,1,now+8);current_room=&cg;r.UpdateVote(now+8);assert(!r.ballot.Active());
  gameplay::vote_receive_ready=false;gameplay::vote_decode=nullptr;master=true;
 }

 {
  gameplay::Runtime r(api);gameplay::runtime=&r;r.alive=M(1);r.go=M(3);r.go_active=M(4);r.master=M(20);r.current_room=M(21);r.mine=M(29);
  N room(&unity,"room"),local(&player,"local");current_room=&room;local_player=&local;
  gameplay::hook_api=&api;gameplay::unity_thread=std::this_thread::get_id();gameplay::scene_method=M(22);gameplay::scene_valid_method=gameplay::scene_loaded_method=M(23);gameplay::room_method=M(21);gameplay::mine_method=M(29);
  gameplay::protected_room=reinterpret_cast<uintptr_t>(O(&room));gameplay::protected_scene=scene.handle;master=owned=true;r.snap.ready=r.snap.authority=r.snap.god=r.snap.god_ready=gameplay::protect=true;
  C item_enum{"","PickUpObject"},inv_class{"","HandlePickUpItemInteractions",&behavior},world_class{"","WorldInteractable",&behavior},manager_class{"","GrannyManager",&behavior};
  F hand{"currentObjectInHand",&item_enum},target{"rgn",&world_class};inv_class.fields={&hand,&target,&pv};world_class.fields={};manager_class.fields={&pv};
  N inv(&inv_class,"inventory"),it(&transform,"inventory"),ig(&go,"inventory"),world(&world_class,"lock"),wt(&transform,"lock"),wg(&go,"lock"),requirements(&list,"requirements"),v(&view,"view");
  Connect(inv,it,ig);Connect(world,wt,wg);inv.refs[&pv]=&v;inv.refs[&target]=&world;inv.bytes[&hand]=25;requirements.values={12,13};list.methods["get_Count:0"]=M(45);list.methods["get_Item:1"]=M(46);assist_list=&requirements;
  r.inventory.Set(O(&inv));gameplay::tool_inventory=r.inventory.h;gameplay::inventory_view=MakeField(&pv);gameplay::assist_hand=MakeField(&hand);gameplay::assist_target=MakeField(&target);gameplay::assist_hooks_ready=true;
  gameplay::original_interaction_ray=NativeRay;gameplay::original_requirements=Requirements;gameplay::original_require_placing=Placing;
  gameplay::InteractionRay(O(&inv),nullptr);assert(observed_hand==12&&!observed_placing&&inv.bytes[&hand]==25&&assisted_uses==1&&!gameplay::assist_scope);
  inv.bytes[&hand]=13;gameplay::InteractionRay(O(&inv),nullptr);assert(observed_hand==13&&inv.bytes[&hand]==13&&!observed_placing);inv.bytes[&hand]=25;
  assist_throw=true;try{gameplay::InteractionRay(O(&inv),nullptr);assert(false);}catch(int){}assist_throw=false;assert(inv.bytes[&hand]==25&&!gameplay::assist_scope);
  assist_fresh=true;gameplay::InteractionRay(O(&inv),nullptr);assist_fresh=false;assert(inv.bytes[&hand]==9);inv.bytes[&hand]=25;
  const int uses=assisted_uses;gameplay::protect=false;gameplay::InteractionRay(O(&inv),nullptr);gameplay::protect=true;assert(observed_hand==25&&observed_placing&&uses==assisted_uses);
  owned=false;gameplay::InteractionRay(O(&inv),nullptr);owned=true;assert(observed_hand==25&&observed_placing);
  gameplay::protected_scene=scene.handle+1;gameplay::InteractionRay(O(&inv),nullptr);gameplay::protected_scene=scene.handle;assert(observed_hand==25&&observed_placing);
  prerequisite=false;gameplay::InteractionRay(O(&inv),nullptr);prerequisite=true;assert(uses==assisted_uses&&inv.bytes[&hand]==25);
  C difficulty_enum{"","GameDifficulty"};F difficulty{"Difficulty",&difficulty_enum,0x10};test_difficulty_field=&difficulty;r.difficulty_field=MakeField(&difficulty);r.snap.difficulty_ready=true;api.field_static_set_value=StaticWrite;
  r.ChangeDifficulty(3);assert(test_difficulty==3&&r.snap.difficulty==3);r.ChangeDifficulty(6);assert(test_difficulty==3);master=false;r.ChangeDifficulty(0);assert(test_difficulty==3);master=true;
  auto& a=*r.actors.emplace_back(std::make_unique<gameplay::Actor>(api,O(&copy),0));r.difficulty_refresh[0]=M(47);gameplay::difficulty_helpers_ready={true,true};gameplay::original_difficulty_helpers[0][0]=OriginalDifficultyHelper;copy.refs[&pv]=&v;a.speed_fields.push_back({MakeField(&speed),1,1,false});
  assert(r.RefreshDifficulty(a)&&copy.floats[&speed]==4&&a.speed_fields[0].base==4&&difficulty_helper_calls==0&&!gameplay::difficulty_target);gameplay::DifficultyHelper<0,0>(O(&copy),nullptr);assert(difficulty_helper_calls==1);refresh_error=true;assert(!r.RefreshDifficulty(a)&&!gameplay::difficulty_target);refresh_error=false;
  C mom_class{"","SpiderMomAIController",&behavior};F routes_field{"wayPoints",&list},wander{"wanderSpeed",&single};mom_class.fields={&pv,&routes_field,&wander};
  N mom(&mom_class,"mom"),mt(&transform,"mom"),mg(&go,"mom"),points(&list,"old routes"),mn(&nav,"mom nav");Connect(mom,mt,mg);mom.refs[&pv]=&v;mom.refs[&routes_field]=&points;points.children={&wt};list.methods[".ctor:0"]=M(48);api.object_new=AllocateForTest;
  gameplay::Actor mom_actor(api,O(&mom),2);mom_actor.nav.Set(O(&mn));mom_actor.speed_fields.push_back({MakeField(&wander),2,2,false});r.difficulty_refresh[2]=M(49);r.nav_set=M(15);
  assert(r.RefreshDifficulty(mom_actor)&&mom.refs[&routes_field]==&points&&points.children.size()==1&&points.children[0]==&wt&&mom_actor.speed_fields[0].base==5);
  refresh_error=true;assert(!r.RefreshDifficulty(mom_actor)&&mom.refs[&routes_field]==&points&&points.children.size()==1&&!gameplay::difficulty_target);refresh_error=false;
  gameplay_bridge::Snapshot bridge;bridge.ready=true;bridge.context=42;gameplay_bridge::Publish(bridge);
  gameplay_bridge::Command command{gameplay_bridge::Kind::Difficulty,42};
  command.kind=gameplay_bridge::Kind::Difficulty;command.item=6;assert(!gameplay_bridge::Queue(command));command.item=0;assert(gameplay_bridge::Queue(command));command.context=41;assert(!gameplay_bridge::Queue(command));gameplay_bridge::Clear();
  gameplay::runtime=nullptr;gameplay::protect=false;gameplay::assist_hooks_ready=false;gameplay::tool_inventory=0;test_difficulty_field=nullptr;
 }
 {
  // Exercise real native visibility wrappers and context-bound lifecycle.
  gameplay::Runtime r(api);gameplay::runtime=&r;gameplay::hook_api=&api;
  r.alive=M(1);r.transform=r.go_transform=M(2);r.go=M(3);r.go_active=r.active_get=M(4);r.is_child=M(19);r.mine=M(29);r.current_room=M(21);r.master=M(20);
  gameplay::unity_thread=std::this_thread::get_id();gameplay::scene_method=M(22);gameplay::scene_valid_method=gameplay::scene_loaded_method=M(23);gameplay::room_method=M(21);gameplay::mine_method=M(29);gameplay::local_player_method=M(24);
  N self(&player,"self"),room(&unity,"room"),v(&view,"self view"),other_view(&view,"remote view");local_player=&self;current_room=&room;
  gameplay::protected_scene=scene.handle;gameplay::protected_room=reinterpret_cast<uintptr_t>(O(&room));owned=master=true;
  C toggle_class{"","HandleLocalSeeTargetToggling",&behavior},inv_class{"","HandlePickUpItemInteractions",&behavior};F see_target{"grannySeeTarget",&go};toggle_class.fields={&pv,&see_target};inv_class.fields={&pv};toggle_class.methods["hpg:2"]=M(50);
  N toggle(&toggle_class,"toggle"),tt(&transform,"player root"),tg(&go,"player model"),marker(&go,"AI marker"),mt(&transform,"marker"),inv(&inv_class,"inventory");
  Connect(toggle,tt,tg);Connect(marker,mt,marker);mt.parent=&tt;toggle.refs={{&pv,&v},{&see_target,&marker}};inv.refs[&pv]=&v;
  r.sight_class=K(&toggle_class);r.sight.Set(O(&toggle));r.inventory.Set(O(&inv));r.snap.ready=r.snap.god_ready=true;
  gameplay::sight_hooks_ready=true;gameplay::original_sight_request=NativeSight;gameplay::original_sight_rpc=NativeSightRpc;
  assert(r.SightOwned());r.SetGod(true);r.UpdateSight();assert(r.snap.invisible&&!marker.active&&tg.active&&sight_local_calls==1&&sight_network_calls==1);
  r.UpdateSight();assert(sight_network_calls==1); // no per-frame RPC flood
  r.sight_next=0;r.UpdateSight();assert(sight_network_calls==2); // late-peer heartbeat
  gameplay::SightRequestHook(O(&toggle),true,true,nullptr);assert(!marker.active&&r.sight_normal);
  gameplay::SightRpcHook(O(&toggle),true,nullptr);assert(!marker.active); // queued native show cannot unhide us
  r.SetGod(false);r.UpdateSight();assert(marker.active&&!r.snap.invisible&&!r.sight_applied);
  marker.active=false;r.SetGod(true);r.UpdateSight();r.SetGod(false);r.UpdateSight();assert(!marker.active); // initially hidden stays hidden
  marker.active=true;r.SetGod(true);r.UpdateSight();gameplay::SightRequestHook(O(&toggle),false,true,nullptr);r.SetGod(false);r.UpdateSight();assert(!marker.active); // native hide request retained
  marker.active=true;r.SetGod(true);r.UpdateSight();sight_error=true;r.SetGod(false);r.UpdateSight();assert(r.sight_applied&&!r.sight_internal);sight_error=false;r.sight_next=0;r.UpdateSight();assert(!r.sight_applied&&marker.active);
  toggle.refs[&pv]=&other_view;const int before=sight_network_calls;r.SetGod(true);r.UpdateSight();assert(!r.snap.invisible&&sight_network_calls==before);toggle.refs[&pv]=&v;
  toggle.refs[&see_target]=&tg;assert(!r.SightOwned()); // never disable the player root/model
  toggle.refs[&see_target]=&marker;r.UpdateSight();assert(!marker.active);
  ++scene.handle;r.ReleaseSight(true);assert(!r.sight_applied&&!marker.active);--scene.handle; // stale context must not restore
  marker.active=true;r.UpdateSight();r.Clear(false);assert(marker.active&&!r.sight_applied); // same-world cleanup restores
  gameplay::runtime=nullptr;gameplay::sight_hooks_ready=false;
 }
 assert(roots.empty());
 {
  recover_clears=recover_resets=recover_routes=0;
  // Native cleanup drops only unavailable victims. Visible targets keep
  // their attack, including when the local player has God enabled.
  gameplay::Runtime r(api);gameplay::runtime=&r;gameplay::hook_api=&api;
  r.alive=M(1);r.transform=r.go_transform=M(2);r.go=M(3);r.go_active=M(4);r.mine=M(29);r.master=M(20);r.is_child=M(19);
  N local(&player,"self"),remote(&player,"remote"),room(&unity,"room"),v(&view,"owned enemy view"),victim(&go,"victim marker"),vt(&transform,"victim"),route_t(&transform,"patrol point");Connect(victim,vt,victim);local_player=&local;current_room=&room;recovery_route=&route_t;
  gameplay::protected_scene=scene.handle;gameplay::protected_room=reinterpret_cast<uintptr_t>(O(&room));r.snap.ready=r.snap.authority=true;master=owned=true;
  C human{"","EnemyAIGrannyMulti",&behavior},human_eye{"","EnemyEye",&behavior},mom_class{"","SpiderMomAIController",&behavior};
  F ep{"enemyEye",&human_eye},chosen{"pka",&player},chosen_grandpa{"pdz",&player},victim_t{"targetTransform",&transform},patrol{"target",&transform},see{"seePlayer",&boolean},caught_now{"playerGetCaught",&boolean},eating{"eatingPlayer",&boolean},hunt{"hunting",&boolean},mom_player{"rnx",&player},weapon{"gotHitByWeapon",&boolean};
  human.fields={&pv,&ep,&victim_t,&patrol,&see,&caught_now,&weapon};human_eye.fields={&chosen,&chosen_grandpa};human_eye.methods={{"eli:0",M(51)},{"dwz:0",M(51)}};human.methods={{"eim:0",M(52)},{"ein:0",M(53)},{"duo:0",M(52)},{"dup:0",M(53)}};
  mom_class.fields={&pv,&patrol,&mom_player,&eating,&hunt,&weapon};mom_class.methods={{"inb:0",M(54)},{"inf:0",M(52)}};
  N enemy(&human,"enemy"),et(&transform,"enemy"),eg(&go,"enemy"),eye_node(&human_eye,"eye");Connect(enemy,et,eg);enemy.refs={{&pv,&v},{&ep,&eye_node},{&victim_t,&vt},{&patrol,&vt}};eye_node.refs[&chosen]=&remote;enemy.flags[&see]=enemy.flags[&caught_now]=true;
  gameplay::Actor actor(api,O(&enemy),0);assert(!r.ReleaseHiddenTarget(actor)&&recover_clears==0);
  victim.active=false;r.settings[0].paused=true;assert(!r.ReleaseHiddenTarget(actor));r.settings[0].paused=false;
  enemy.flags[&weapon]=true;assert(!r.ReleaseHiddenTarget(actor));enemy.flags[&weapon]=false;
  master=false;assert(!r.ReleaseHiddenTarget(actor));master=true;owned=false;assert(!r.ReleaseHiddenTarget(actor));owned=true;
  assert(r.ReleaseHiddenTarget(actor)&&!eye_node.refs[&chosen]&&!enemy.flags[&caught_now]&&!enemy.flags[&see]&&!enemy.refs[&victim_t]&&enemy.refs[&patrol]==&route_t);
  assert(recover_clears==1&&recover_resets==1&&recover_routes==1);assert(!r.ReleaseHiddenTarget(actor));
  actor.kind=1;eye_node.refs[&chosen_grandpa]=&remote;enemy.refs[&victim_t]=&vt;enemy.flags[&caught_now]=true;assert(r.ReleaseHiddenTarget(actor)&&!eye_node.refs[&chosen_grandpa]&&!enemy.flags[&caught_now]);
  N mom(&mom_class,"mom"),mot(&transform,"mom"),mog(&go,"mom");Connect(mom,mot,mog);mom.refs={{&pv,&v},{&patrol,&vt},{&mom_player,&remote}};mom.flags[&eating]=mom.flags[&hunt]=true;
  gameplay::Actor mom_actor(api,O(&mom),2);assert(r.ReleaseHiddenTarget(mom_actor)&&!mom.refs[&mom_player]&&!mom.flags[&hunt]&&!mom.flags[&eating]&&mom.refs[&patrol]==&route_t);
  // The category pause contains all actor instances of that kind, not only clones.
  r.actors.push_back(std::make_unique<gameplay::Actor>(api,O(&enemy),0));
  N another(&human,"second Granny");r.actors.push_back(std::make_unique<gameplay::Actor>(api,O(&another),0));r.actors.push_back(std::make_unique<gameplay::Actor>(api,O(&mom),2));
  r.settings[0].paused=true;assert(gameplay::EnemyPaused(O(&enemy),0)&&gameplay::EnemyPaused(O(&another),0)&&!gameplay::EnemyPaused(O(&mom),2));
  r.settings[0].paused=false;assert(!gameplay::EnemyPaused(O(&enemy),0)&&!gameplay::EnemyPaused(O(&another),0));
  gameplay::runtime=nullptr;
 }
 assert(roots.empty());
 std::cout<<"AI repair: no-original local Granny/Grandpa/Mom binding, unique scene DaysManager, accepted sound callback return/GC/dedupe, stationary/attack timeout recovery with pause/hit guards, own player speed noncompounding/base restoration/ownership: PASS\n";
 std::cout<<"Clone extensions: first-obstruction door RPC/cooldown/lock guards, Grandpa stripped fields, native lost-sight patrol, actual noise dedupe/investigation/sight priority, client chat-only vote/prank/draft/cooldown/tie/context: PASS\n";
 std::cout<<"AI visibility: own-target native local/RPC lifecycle, request preservation, rate limit, retries, model/ownership/context guards; native hidden-victim release for all three enemies and category pause: PASS\n";
 std::cout<<"Progression: own God interaction item/consumption scope, exception/fresh-hand restoration, prerequisite/ownership/scene guards, difficulty context/enum checks, retained combat helpers and Mom route restoration: PASS\n";
 assert(roots.empty());
 std::cout<<"Enemy behavior: clone-local camera/AI remapping, scene-only bindings, independent target lists, animation enable/speed, hierarchy rejection, delayed God attack release preserving chase, other-player/host/death guards: PASS\n";
}
