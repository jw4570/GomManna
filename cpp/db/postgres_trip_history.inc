// Included in the PostgreSQL adapter's private namespace.
// Live APIs retain their original OFF/epoch rules. History requires separate opt-in.
const std::string kTripExpiry =
    "least(r.ended_at+interval '48 hours',r.all_sharing_stopped_at+interval '48 hours',"
    "r.last_activity_at+interval '48 hours 10 minutes')";
TripSummary PostgresRepository::locked_trip(const Id& actor,const Id& room_id,TimeMs now) {
    require_now(now);
    auto room=locked_room(room_id,true);
    locked_member(actor,room_id,true);
    if(room.active) reject(ErrorCode::conflict,"trip has not ended");
    auto row=exec(conn_.get(),"select round(extract(epoch from "+kTripExpiry+")*1000)::bigint expires "
        "from gommanna.rooms r where r.id=$1::uuid",{room_id});
    const auto expiry=integer_at(row.get(),0,"expires");
    // Deny at the deadline, even when the background cleanup worker has not run.
    if(now>=expiry) reject(ErrorCode::not_found,"trip expired");
    return {room,expiry};
}
Member PostgresRepository::trip_target(const Id& actor,const Id& room_id,const Id& target_id) {
    auto own=locked_member(actor,room_id,true);
    auto row=exec(conn_.get(),"select "+kMemberColumns+",m.trip_history_shared from gommanna.room_members m "
        "where m.room_id=$1::uuid and m.id=$2::uuid",{room_id,target_id});
    if(!PQntuples(row.get())) reject(ErrorCode::not_found,"member not found");
    auto target=member_at(row.get(),0);
    if(own.id!=target.id && (own.left_at_ms || target.left_at_ms || !bool_at(row.get(),0,"trip_history_shared")))
        reject(ErrorCode::forbidden,"trip history is not shared");
    return target;
}
Result<std::vector<TripSummary>> PostgresRepository::list_trips(const Id& actor,int offset,int limit,TimeMs now) {
    return guarded<std::vector<TripSummary>>([&] {
        require_text(actor);require_now(now);
        if(offset<0 || offset>100000 || limit<1 || limit>100) reject(ErrorCode::invalid_argument,"invalid trip page");
        auto rows=exec(conn_.get(),"select "+kRoomColumns+",round(extract(epoch from "+kTripExpiry+")*1000)::bigint expires "
            "from gommanna.rooms r where r.status='ended' and "+kTripExpiry+">to_timestamp($2::double precision/1000) "
            "and exists(select 1 from gommanna.room_members m where m.room_id=r.id and m.user_id=$1::uuid) "
            "order by r.ended_at desc,r.id limit $3::integer offset $4::integer",
            {actor,std::to_string(now),std::to_string(limit),std::to_string(offset)});
        std::vector<TripSummary> result;
        for(int i=0;i<PQntuples(rows.get());++i)result.push_back({room_at(rows.get(),i),integer_at(rows.get(),i,"expires")});
        return result;
    });
}
Result<TripDetail> PostgresRepository::get_trip(const Id& actor,const Id& room_id,TimeMs now) {
    return guarded<TripDetail>([&] {
        Transaction tx(conn_.get());TripDetail result;result.trip=locked_trip(actor,room_id,now);
        auto own=locked_member(actor,room_id,true);
        auto members=exec(conn_.get(),"select "+kMemberColumns+",m.trip_history_shared from gommanna.room_members m "
            "where room_id=$1::uuid and (id=$2::uuid or ($3::boolean and left_at is null)) order by joined_at,id",
            {room_id,own.id,own.left_at_ms?"false":"true"});
        for(int i=0;i<PQntuples(members.get());++i)
            result.members.push_back({member_at(members.get(),i),bool_at(members.get(),i,"trip_history_shared")});
        auto marks=exec(conn_.get(),"select "+kMarkColumns+"from gommanna.map_marks k where room_id=$1::uuid "
            "and exists(select 1 from gommanna.room_members m where m.room_id=k.room_id and m.user_id=k.creator_id "
            "and (m.id=$2::uuid or ($3::boolean and m.left_at is null and m.trip_history_shared))) order by created_at,id",
            {room_id,own.id,own.left_at_ms?"false":"true"});
        for(int i=0;i<PQntuples(marks.get());++i)result.marks.push_back(mark_at(marks.get(),i));
        tx.commit();return result;
    });
}
Result<bool> PostgresRepository::get_trip_sharing(const Id& actor,const Id& room_id,TimeMs now) {
    return guarded<bool>([&] {
        require_now(now);Transaction tx(conn_.get());auto room=locked_room(room_id,true);
        auto own=locked_member(actor,room_id,true);
        if(!room.active)locked_trip(actor,room_id,now);
        auto row=exec(conn_.get(),"select trip_history_shared from gommanna.room_members where id=$1::uuid",{own.id});
        bool result=bool_at(row.get(),0,"trip_history_shared");tx.commit();return result;
    });
}
Result<Unit> PostgresRepository::set_trip_sharing(const Id& actor,const Id& room_id,bool enabled,TimeMs now) {
    return guarded<Unit>([&] {
        require_now(now);Transaction tx(conn_.get());auto room=locked_room(room_id,true);
        auto own=locked_member(actor,room_id,true);
        if(!room.active)locked_trip(actor,room_id,now);
        if(enabled && own.left_at_ms)reject(ErrorCode::forbidden,"departed member cannot share history");
        exec(conn_.get(),"update gommanna.room_members set trip_history_shared=$2::boolean where id=$1::uuid",
            {own.id,enabled?"true":"false"});
        tx.commit();return Unit{};
    });
}
Result<RoutePage> PostgresRepository::get_trip_route(const Id& actor,const Id& room_id,const Id& target_id,
    std::int64_t after,int limit,TimeMs now) {
    return guarded<RoutePage>([&] {
        if(after<0 || limit<1 || limit>500)reject(ErrorCode::invalid_argument,"invalid route page");
        Transaction tx(conn_.get());locked_trip(actor,room_id,now);trip_target(actor,room_id,target_id);
        auto rows=exec(conn_.get(),"select l.id::text point_id,"+kLocationColumns+
            "from gommanna.route_points l where l.room_id=$1::uuid and l.member_id=$2::uuid and l.id>$3::bigint "
            "order by l.id limit $4::integer",{room_id,target_id,std::to_string(after),std::to_string(limit+1)});
        RoutePage result;
        for(int i=0;i<PQntuples(rows.get());++i)result.points.push_back({integer_at(rows.get(),i,"point_id"),target_id,location_at(rows.get(),i)});
        if(result.points.size()>static_cast<std::size_t>(limit)){result.points.pop_back();result.next_after_id=result.points.back().id;}
        tx.commit();return result;
    });
}
const std::string kTripNoteColumns=
    "n.id::text record_id,n.member_id::text,n.note_id::text n_id,n.text n_text,"
    "round(extract(epoch from n.written_at)*1000)::bigint n_written,"
    "n.origin_latitude n_lat,n.origin_longitude n_lon,"
    "round(extract(epoch from n.origin_captured_at)*1000)::bigint n_captured,"
    "n.sharing_epoch n_epoch,n.photo_key n_photo_key,"
    "round(extract(epoch from n.photo_uploaded_at)*1000)::bigint n_photo_uploaded ";
Result<TripNotePage> PostgresRepository::get_trip_notes(const Id& actor,const Id& room_id,
    std::int64_t after,int limit,TimeMs now) {
    return guarded<TripNotePage>([&] {
        if(after<0 || limit<1 || limit>200)reject(ErrorCode::invalid_argument,"invalid notes page");
        Transaction tx(conn_.get());locked_trip(actor,room_id,now);auto own=locked_member(actor,room_id,true);
        auto rows=exec(conn_.get(),"select "+kTripNoteColumns+"from gommanna.trip_notes n "
            "join gommanna.room_members m on m.id=n.member_id where n.room_id=$1::uuid and n.id>$2::bigint "
            "and (m.id=$3::uuid or ($4::boolean and m.left_at is null and m.trip_history_shared)) "
            "order by n.id limit $5::integer",{room_id,std::to_string(after),own.id,own.left_at_ms?"false":"true",std::to_string(limit+1)});
        TripNotePage result;
        for(int i=0;i<PQntuples(rows.get());++i)result.notes.push_back({integer_at(rows.get(),i,"record_id"),string_at(rows.get(),i,"member_id"),note_at(rows.get(),i)});
        if(result.notes.size()>static_cast<std::size_t>(limit)){result.notes.pop_back();result.next_after_id=result.notes.back().record_id;}
        tx.commit();return result;
    });
}
Result<PhotoRef> PostgresRepository::get_trip_photo(const Id& actor,const Id& room_id,std::int64_t id,TimeMs now) {
    return guarded<PhotoRef>([&] {
        if(id<1)reject(ErrorCode::invalid_argument,"invalid record id");
        Transaction tx(conn_.get());locked_trip(actor,room_id,now);
        auto row=exec(conn_.get(),"select member_id::text,photo_key,round(extract(epoch from photo_uploaded_at)*1000)::bigint uploaded "
            "from gommanna.trip_notes where room_id=$1::uuid and id=$2::bigint",{room_id,std::to_string(id)});
        if(!PQntuples(row.get()) || is_null(row.get(),0,"photo_key"))reject(ErrorCode::not_found,"photo not found");
        trip_target(actor,room_id,string_at(row.get(),0,"member_id"));
        PhotoRef result{string_at(row.get(),0,"photo_key"),integer_at(row.get(),0,"uploaded")};tx.commit();return result;
    });
}
Result<Unit> PostgresRepository::delete_trip_note(const Id& actor,const Id& room_id,std::int64_t id,TimeMs now) {
    return guarded<Unit>([&] {
        require_now(now);if(id<1)reject(ErrorCode::invalid_argument,"invalid record id");
        Transaction tx(conn_.get());locked_room(room_id,true);auto own=locked_member(actor,room_id,true);
        auto row=exec(conn_.get(),"select note_id::text,round(extract(epoch from written_at)*1000)::bigint written "
            "from gommanna.trip_notes where room_id=$1::uuid and id=$2::bigint and member_id=$3::uuid for update",
            {room_id,std::to_string(id),own.id});
        if(!PQntuples(row.get()))reject(ErrorCode::not_found,"own record not found");
        // Deleting a historical revision must not delete a newer live note.
        exec(conn_.get(),"delete from gommanna.member_notes where member_id=$1::uuid and id=$2::uuid "
            "and written_at=to_timestamp($3::double precision/1000)",
            {own.id,string_at(row.get(),0,"note_id"),std::to_string(integer_at(row.get(),0,"written"))});
        exec(conn_.get(),"delete from gommanna.trip_notes where id=$1::bigint",{std::to_string(id)});
        tx.commit();return Unit{};
    });
}
