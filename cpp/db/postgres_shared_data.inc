// Included by postgres_repository.cpp inside its private namespace.
User profile_at(PGresult* r,int row) {
    return {string_at(r,row,"user_id"),integer_at(r,row,"created_ms"),string_at(r,row,"animal_icon"),0};
}
const std::string kProfileColumns=
    "id::text user_id,round(extract(epoch from created_at)*1000)::bigint created_ms,animal_icon ";
const std::string kMarkColumns=
    "id::text,room_id::text,creator_id::text,name,latitude,longitude,"
    "round(extract(epoch from created_at)*1000)::bigint created_ms,"
    "round(extract(epoch from updated_at)*1000)::bigint updated_ms ";
MapMark mark_at(PGresult* r,int row) {
    return {string_at(r,row,"id"),string_at(r,row,"room_id"),string_at(r,row,"creator_id"),
        string_at(r,row,"name"),{double_at(r,row,"latitude"),double_at(r,row,"longitude")},
        integer_at(r,row,"created_ms"),integer_at(r,row,"updated_ms")};
}
Result<User> PostgresRepository::get_profile(const Id& actor) {
    return guarded<User>([&] {
        require_text(actor);
        auto r=exec(conn_.get(),"select "+kProfileColumns+"from gommanna.users where id=$1::uuid",{actor});
        if(!PQntuples(r.get())) reject(ErrorCode::unauthenticated,"guest not found");
        return profile_at(r.get(),0);
    });
}
Result<User> PostgresRepository::update_profile(const Id& actor,const std::string& icon) {
    return guarded<User>([&] {
        require_text(actor);
        if(!valid_animal_icon(icon)) reject(ErrorCode::invalid_argument,"unknown animal icon");
        auto r=exec(conn_.get(),"update gommanna.users set animal_icon=$2 where id=$1::uuid returning "+kProfileColumns,{actor,icon});
        if(!PQntuples(r.get())) reject(ErrorCode::unauthenticated,"guest not found");
        return profile_at(r.get(),0);
    });
}
Result<Unit> PostgresRepository::delete_user(const Id& actor,TimeMs now) {
    return guarded<Unit>([&] {
        require_now(now);require_text(actor);
        Transaction tx(conn_.get());
        auto user=exec(conn_.get(),"select id from gommanna.users where id=$1::uuid for update",{actor});
        if(!PQntuples(user.get())) reject(ErrorCode::unauthenticated,"guest not found");
        // Deterministic room locking also serializes deletion against location/photo updates.
        auto affected=exec(conn_.get(),"select "+kRoomColumns+"from gommanna.rooms r "
            "where r.host_id=$1::uuid or exists(select 1 from gommanna.room_members m "
            "where m.room_id=r.id and m.user_id=$1::uuid) order by r.id for update",{actor});
        std::vector<Id> active;
        for(int i=0;i<PQntuples(affected.get());++i) {
            auto room=room_at(affected.get(),i);
            if(room.active && room.host_id==actor) {
                auto successor=exec(conn_.get(),"select user_id::text from gommanna.room_members "
                    "where room_id=$1::uuid and user_id<>$2::uuid and left_at is null order by joined_at,id limit 1",{room.id,actor});
                if(PQntuples(successor.get())) {
                    exec(conn_.get(),"update gommanna.rooms set host_id=$2::uuid where id=$1::uuid",
                        {room.id,string_at(successor.get(),0,"user_id")});
                } else {
                    exec(conn_.get(),"update gommanna.rooms set status='ended',ended_at=to_timestamp($2::double precision/1000),"
                        "host_id=null where id=$1::uuid",{room.id,std::to_string(now)});
                    exec(conn_.get(),"update gommanna.room_members set is_sharing=false,sharing_epoch=sharing_epoch+1 "
                        "where room_id=$1::uuid",{room.id});
                    room.active=false;
                }
            }
            if(room.active) active.push_back(room.id);
        }
        // Cascades delete sessions, memberships, own routes/notes/marks. The existing
        // note trigger queues private Storage objects for retryable physical deletion.
        exec(conn_.get(),"delete from gommanna.users where id=$1::uuid",{actor});
        for(const auto& room:active) refresh_retention(room,now);
        tx.commit();
        return Unit{};
    });
}
Result<Unit> PostgresRepository::delete_member_data(const Id& actor,const Id& room_id,bool route_only,TimeMs now) {
    return guarded<Unit>([&] {
        require_now(now);
        Transaction tx(conn_.get());
        locked_room(room_id,true);
        auto member=locked_member(actor,room_id,true);
        exec(conn_.get(),"delete from gommanna.route_points where member_id=$1::uuid",{member.id});
        if(!route_only) {
            exec(conn_.get(),"delete from gommanna.member_locations where member_id=$1::uuid",{member.id});
            exec(conn_.get(),"delete from gommanna.member_notes where member_id=$1::uuid",{member.id});
            exec(conn_.get(),"delete from gommanna.map_marks where room_id=$1::uuid and creator_id=$2::uuid",{room_id,actor});
            exec(conn_.get(),"update gommanna.room_members set is_sharing=false,sharing_epoch=sharing_epoch+1,"
                "battery_percent=null,is_charging=null,device_epoch=null,device_updated_at=null where id=$1::uuid",{member.id});
            refresh_retention(room_id,now);
        }
        tx.commit();
        return Unit{};
    });
}
Result<DeviceStatus> PostgresRepository::update_device_status(
    const Id& actor,const Id& room_id,const DeviceStatusDraft& draft,TimeMs now) {
    return guarded<DeviceStatus>([&] {
        require_now(now);
        if(draft.battery_percent && (*draft.battery_percent<0 || *draft.battery_percent>100))
            reject(ErrorCode::invalid_argument,"invalid battery percentage");
        Transaction tx(conn_.get());
        locked_room(room_id);
        auto member=locked_member(actor,room_id);
        if(!member.is_sharing) reject(ErrorCode::sharing_off,"sharing is off");
        if(draft.sharing_epoch!=member.sharing_epoch) reject(ErrorCode::stale_update,"old sharing epoch");
        exec(conn_.get(),"update gommanna.room_members set battery_percent=$2::integer,is_charging=$3::boolean,"
            "device_updated_at=to_timestamp($4::double precision/1000),device_epoch=$5::bigint where id=$1::uuid",
            {member.id,draft.battery_percent?Param{std::to_string(*draft.battery_percent)}:std::nullopt,
             draft.is_charging?Param{*draft.is_charging?"true":"false"}:std::nullopt,
             std::to_string(now),std::to_string(member.sharing_epoch)});
        refresh_retention(room_id,now,true);
        tx.commit();
        return DeviceStatus{draft.battery_percent,draft.is_charging,now,member.sharing_epoch};
    });
}
Result<std::vector<MapMark>> PostgresRepository::list_marks(const Id& actor,const Id& room_id) {
    return guarded<std::vector<MapMark>>([&] {
        Transaction tx(conn_.get());locked_room(room_id);locked_member(actor,room_id);
        auto r=exec(conn_.get(),"select "+kMarkColumns+"from gommanna.map_marks k where k.room_id=$1::uuid "
            "and exists(select 1 from gommanna.room_members m where m.room_id=k.room_id "
            "and m.user_id=k.creator_id and m.left_at is null) order by created_at,id",{room_id});
        std::vector<MapMark> result;
        for(int i=0;i<PQntuples(r.get());++i) result.push_back(mark_at(r.get(),i));
        tx.commit();return result;
    });
}
Result<MapMark> PostgresRepository::create_mark(const Id& actor,const Id& room_id,const MapMarkDraft& draft,TimeMs now) {
    return guarded<MapMark>([&] {
        require_now(now);require_text(draft.name);require_coordinate(draft.coordinate);
        Transaction tx(conn_.get());locked_room(room_id);locked_member(actor,room_id);
        auto count=exec(conn_.get(),"select count(*)::text count from gommanna.map_marks where room_id=$1::uuid",{room_id});
        if(integer_at(count.get(),0,"count")>=200) reject(ErrorCode::conflict,"room mark limit reached");
        auto r=exec(conn_.get(),"insert into gommanna.map_marks(room_id,creator_id,name,latitude,longitude,created_at,updated_at) "
            "values($1::uuid,$2::uuid,btrim($3),$4::float8,$5::float8,to_timestamp($6::double precision/1000),"
            "to_timestamp($6::double precision/1000)) returning "+kMarkColumns,
            {room_id,actor,draft.name,number(draft.coordinate.latitude),number(draft.coordinate.longitude),std::to_string(now)});
        auto result=mark_at(r.get(),0);refresh_retention(room_id,now);tx.commit();return result;
    });
}
Result<MapMark> PostgresRepository::rename_mark(const Id& actor,const Id& room_id,const Id& mark_id,const std::string& name,TimeMs now) {
    return guarded<MapMark>([&] {
        require_now(now);require_text(name);require_text(mark_id);
        Transaction tx(conn_.get());auto room=locked_room(room_id);locked_member(actor,room_id);
        auto mark=exec(conn_.get(),"select creator_id::text from gommanna.map_marks where room_id=$1::uuid and id=$2::uuid for update",{room_id,mark_id});
        if(!PQntuples(mark.get())) reject(ErrorCode::not_found,"mark not found");
        if(string_at(mark.get(),0,"creator_id")!=actor && room.host_id!=actor) reject(ErrorCode::forbidden,"mark creator or host only");
        auto r=exec(conn_.get(),"update gommanna.map_marks set name=btrim($3),updated_at=to_timestamp($4::double precision/1000) "
            "where room_id=$1::uuid and id=$2::uuid returning "+kMarkColumns,{room_id,mark_id,name,std::to_string(now)});
        auto result=mark_at(r.get(),0);refresh_retention(room_id,now);tx.commit();return result;
    });
}
Result<Unit> PostgresRepository::delete_mark(const Id& actor,const Id& room_id,const Id& mark_id) {
    return guarded<Unit>([&] {
        require_text(mark_id);
        Transaction tx(conn_.get());auto room=locked_room(room_id);locked_member(actor,room_id);
        auto r=exec(conn_.get(),"select creator_id::text from gommanna.map_marks where room_id=$1::uuid and id=$2::uuid for update",{room_id,mark_id});
        if(!PQntuples(r.get())) reject(ErrorCode::not_found,"mark not found");
        if(string_at(r.get(),0,"creator_id")!=actor && room.host_id!=actor) reject(ErrorCode::forbidden,"mark creator or host only");
        exec(conn_.get(),"delete from gommanna.map_marks where room_id=$1::uuid and id=$2::uuid",{room_id,mark_id});
        tx.commit();return Unit{};
    });
}
Result<RoutePage> PostgresRepository::get_route(
    const Id& actor,const Id& room_id,const Id& member_id,std::int64_t after,int limit,TimeMs now) {
    return guarded<RoutePage>([&] {
        require_now(now);require_text(member_id);
        if(after<0 || limit<1 || limit>500) reject(ErrorCode::invalid_argument,"invalid route page");
        Transaction tx(conn_.get());auto room=locked_room(room_id,true);
        auto own=locked_member(actor,room_id,true);
        const auto target_id=member_id.empty()?own.id:member_id;
        const bool self=own.id==target_id;
        if(!self && (!room.active || own.left_at_ms)) reject(ErrorCode::forbidden,"active membership required");
        auto target=exec(conn_.get(),"select "+kMemberColumns+"from gommanna.room_members m "
            "where room_id=$1::uuid and id=$2::uuid",{room_id,target_id});
        if(!PQntuples(target.get())) reject(ErrorCode::not_found,"member not found");
        const auto member=member_at(target.get(),0);
        RoutePage result;
        if(!self && (member.left_at_ms || !member.is_sharing)) {tx.commit();return result;}
        auto rows=exec(conn_.get(),"select l.id::text point_id,"+kLocationColumns+
            "from gommanna.route_points l where l.room_id=$1::uuid and l.member_id=$2::uuid and l.id>$3::bigint "
            "and ($4::boolean or l.sharing_epoch=$5::bigint) order by l.id limit $6::integer",
            {room_id,target_id,std::to_string(after),self?"true":"false",
             std::to_string(member.sharing_epoch),std::to_string(limit+1)});
        for(int i=0;i<PQntuples(rows.get());++i)
            result.points.push_back({integer_at(rows.get(),i,"point_id"),target_id,location_at(rows.get(),i)});
        if(result.points.size()>static_cast<std::size_t>(limit)) {
            result.points.pop_back();result.next_after_id=result.points.back().id;
        }
        tx.commit();return result;
    });
}
Result<Unit> PostgresRepository::collect_expired_data(TimeMs now) {
    return guarded<Unit>([&] {
        require_now(now);
        Transaction tx(conn_.get());
        const auto stamp=std::to_string(now);
        // Only this pass's deadlines; never infer inactivity from clients' clocks.
        auto due=exec(conn_.get(),"select "+kRoomColumns+"from gommanna.rooms r where "
            "(r.status='ended' and r.ended_at<=to_timestamp($1::double precision/1000)-interval '48 hours') or "
            "(not r.retention_purged and ("
              "r.all_sharing_stopped_at<=to_timestamp($1::double precision/1000)-interval '48 hours' or "
              "r.last_activity_at<=to_timestamp($1::double precision/1000)-interval '48 hours 10 minutes')) "
            "order by r.id limit 100 for update skip locked",{stamp});
        for(int i=0;i<PQntuples(due.get());++i) {
            const auto room=room_at(due.get(),i);
            if(!room.active && room.ended_at_ms && now-*room.ended_at_ms>=kRetentionMs) {
                exec(conn_.get(),"delete from gommanna.rooms where id=$1::uuid",{room.id});
                continue;
            }
            // A departed member's content has the explicit room-ended deadline,
            // not another participant's sharing-OFF/inactivity deadline.
            for(const auto* table:{"member_locations","member_notes","route_points"})
                exec(conn_.get(),std::string("delete from gommanna.")+table+
                    " x using gommanna.room_members m where x.member_id=m.id and m.room_id=$1::uuid and m.left_at is null",{room.id});
            exec(conn_.get(),"delete from gommanna.map_marks k using gommanna.room_members m "
                "where k.room_id=$1::uuid and m.room_id=k.room_id and m.user_id=k.creator_id and m.left_at is null",{room.id});
            exec(conn_.get(),"update gommanna.room_members set battery_percent=null,is_charging=null,"
                "device_epoch=null,device_updated_at=null where room_id=$1::uuid and left_at is null",{room.id});
            exec(conn_.get(),"update gommanna.rooms set retention_purged=true where id=$1::uuid",{room.id});
        }
        exec(conn_.get(),"delete from gommanna.guest_sessions where expires_at<=to_timestamp($1::double precision/1000) "
            "or revoked_at is not null",{stamp});
        tx.commit();return Unit{};
    });
}
