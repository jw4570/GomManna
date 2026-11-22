// Completed-trip endpoints. They do not relax the live sharing endpoints.
auto trip_json=[](const TripSummary& t)->json {
  // Invite codes are deliberately not returned in historical lists.
  return {{"room_id",t.room.id},{"title",t.room.title},{"created_at_ms",t.room.created_at_ms},
    {"ended_at_ms",t.room.ended_at_ms},{"expires_at_ms",t.expires_at_ms}};
};
auto page_args=[](const httplib::Request& req,int maximum) {
  for(const auto& [key,value]:req.params)
    if(key!="after_id" && key!="limit")throw std::invalid_argument("Unknown query parameter");
  auto after=query_integer(req,"after_id",0),limit=query_integer(req,"limit",100);
  if(after<0 || limit<1 || limit>maximum)throw std::invalid_argument("Invalid page");
  return std::pair{after,static_cast<int>(limit)};
};
protect(server,*repo,"GET","/v1/me/trips",[&](auto& req,auto& r,const Id& actor){
  for(const auto& [key,value]:req.params)
    if(key!="offset" && key!="limit")throw std::invalid_argument("Unknown query parameter");
  auto offset=query_integer(req,"offset",0),limit=query_integer(req,"limit",20);
  if(offset<0 || offset>100000 || limit<1 || limit>100)throw std::invalid_argument("Invalid page");
  output(r,repo->list_trips(actor,static_cast<int>(offset),static_cast<int>(limit),now_ms()),
    [&](const std::vector<TripSummary>& trips)->json {
      json values=json::array();for(const auto& t:trips)values.push_back(trip_json(t));
      return {{"trips",values},{"offset",offset},{"limit",limit}};
    });
});
protect(server,*repo,"GET",base+"/trip-sharing",[&](auto& req,auto& r,const Id& actor){
  output(r,repo->get_trip_sharing(actor,normalize_uuid(req.matches[1]),now_ms()),
    [](bool shared)->json{return {{"history_shared",shared}};});
});
protect(server,*repo,"PUT",base+"/trip-sharing",[&](auto& req,auto& r,const Id& actor){
  auto j=body(req,{"enabled"});const auto enabled=json_boolean(j,"enabled");
  output(r,repo->set_trip_sharing(actor,normalize_uuid(req.matches[1]),enabled,now_ms()),
    [&](Unit)->json{return {{"history_shared",enabled}};});
});
const std::string trip_base=R"(/v1/trips/([0-9a-fA-F-]{36}))";
protect(server,*repo,"GET",trip_base,[&](auto& req,auto& r,const Id& actor){
  output(r,repo->get_trip(actor,normalize_uuid(req.matches[1]),now_ms()),[&](const TripDetail& t)->json {
    json members=json::array(),marks=json::array();
    for(const auto& m:t.members)members.push_back({{"member_id",m.member.id},{"user_id",m.member.user_id},
      {"nickname",m.member.nickname},{"animal_icon",m.member.animal_icon},{"joined_at_ms",m.member.joined_at_ms},
      {"left_at_ms",m.member.left_at_ms},{"history_shared",m.history_shared}});
    for(const auto& m:t.marks)marks.push_back(map_mark(m));
    return {{"trip",trip_json(t.trip)},{"members",members},{"marks",marks}};
  });
});
protect(server,*repo,"GET",trip_base+R"(/members/([0-9a-fA-F-]{36})/route)",[&](auto& req,auto& r,const Id& actor){
  auto [after,limit]=page_args(req,500);
  output(r,repo->get_trip_route(actor,normalize_uuid(req.matches[1]),normalize_uuid(req.matches[2]),after,limit,now_ms()),
    [](const RoutePage& page)->json {
      json points=json::array();for(const auto& p:page.points){auto v=location(p.location);v["point_id"]=p.id;v["member_id"]=p.member_id;points.push_back(v);}
      return {{"points",points},{"next_after_id",page.next_after_id}};
    });
});
protect(server,*repo,"GET",trip_base+"/notes",[&](auto& req,auto& r,const Id& actor){
  auto [after,limit]=page_args(req,200);auto rid=normalize_uuid(req.matches[1]);
  output(r,repo->get_trip_notes(actor,rid,after,limit,now_ms()),[&](const TripNotePage& page)->json {
    json notes=json::array();for(const auto& n:page.notes){
      auto v=note(n.note,rid,"");v["record_id"]=n.record_id;v["member_id"]=n.member_id;
      v["photo_path"]=n.note.photo?json("/v1/trips/"+rid+"/notes/"+std::to_string(n.record_id)+"/photo"):json(nullptr);
      notes.push_back(v);
    }
    return {{"notes",notes},{"next_after_id",page.next_after_id}};
  });
});
const auto trip_note_path=trip_base+R"(/notes/([1-9][0-9]{0,17}))";
protect(server,*repo,"DELETE",trip_note_path,[&](auto& req,auto& r,const Id& actor){
  output(r,repo->delete_trip_note(actor,normalize_uuid(req.matches[1]),std::stoll(req.matches[2]),now_ms()),
    [](Unit)->json{return {{"deleted",true},{"storage_cleanup","queued"}};},202);
});
protect(server,*repo,"GET",trip_note_path+"/photo",[&](auto& req,auto& r,const Id& actor){
  if(!photos){fail(r,{ErrorCode::storage_unavailable,"Photo storage is not configured"});return;}
  auto result=photos->download_trip(actor,normalize_uuid(req.matches[1]),std::stoll(req.matches[2]),now_ms());
  if(auto* error=std::get_if<Error>(&result)){fail(r,*error);return;}
  const auto& photo=std::get<SanitizedPhoto>(result);
  r.status=200;r.set_header("Cache-Control","no-store");r.set_header("X-Content-Type-Options","nosniff");r.set_content(photo.bytes,photo.content_type);
});
