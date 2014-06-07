#include "TWTrapAIEcology.h"
#include "ScriptLib.h"

/* =============================================================================
 *  TWTrapAIEcology Impmementation - protected members
 */

/* ------------------------------------------------------------------------
 *  Initialisation related
 */

void TWTrapAIEcology::init(int time)
{
    TWBaseTrap::init(time);

    bool starton = false;

    // Fetch the contents of the object's design note
    char *design_note = GetObjectParams(ObjId());

    if(!design_note) {
        debug_printf(DL_WARNING, "No Editor -> Design Note. Falling back on defaults.");

        // Assume initial start is off
        enabled.Init(0);

    } else {
        // How many AIs can be spawned?
        poplimit = get_scriptparam_int(design_note, "Population", 1);

        // How often should the ecology update?
        refresh  = get_scriptparam_int(design_note, "Rate", 500);

        // Start on? Note that this will only have any effect the first time the script
        // does the init. After this point, the previous enabled state takes over.
        starton = get_scriptparam_bool(design_note, "StartOn", false);
        enabled.Init(starton ? 1 : 0);

        g_pMalloc -> Free(design_note);
    }

    // If the ecology is active, start it going
    if(int(enabled)) {
        start_timer(true);
    }
}


/* ------------------------------------------------------------------------
 *  Message handling
 */

TWBaseScript::MsgStatus TWTrapAIEcology::on_message(sScrMsg* msg, cMultiParm& reply)
{
    // Call the superclass to let it handle any messages it needs to
    MsgStatus result = TWBaseTrap::on_message(msg, reply);
    if(result != MS_CONTINUE) return result;

    if(!::_stricmp(msg -> message, "Timer")) {
        return on_timer(static_cast<sScrTimerMsg*>(msg), reply);
    }

    return result;
}


TWBaseScript::MsgStatus TWTrapAIEcology::on_onmsg(sScrMsg* msg, cMultiParm& reply)
{
    // Only activate the ecology if it is not already active
    if(!int(enabled)) {
        if(debug_enabled())
            debug_printf(DL_DEBUG, "Received on message, activating ecology.");

        enabled = 1;

        // This shouldn't be necessary, as the timer should already be off, but force it
        stop_timer();

        // And do the first check before starting the timer.
        attempt_spawn(msg);
        start_timer();
    } else if(debug_enabled()) {
        debug_printf(DL_DEBUG, "Received on message, ignoring as ecology is already active.");
    }

    return MS_CONTINUE;
}


TWBaseScript::MsgStatus TWTrapAIEcology::on_offmsg(sScrMsg* msg, cMultiParm& reply)
{
    // Only deactivate the ecology if it is active
    if(int(enabled)) {
        if(debug_enabled())
            debug_printf(DL_DEBUG, "Received off message, deactivating ecology.");

        enabled = 0;
        stop_timer();
    } else if(debug_enabled()) {
        debug_printf(DL_DEBUG, "Received off message, ignoring as ecology is already inactive.");
    }

    return MS_CONTINUE;
}


TWBaseScript::MsgStatus TWTrapAIEcology::on_timer(sScrTimerMsg* msg, cMultiParm& reply)
{
    // Only bother doing anything if the timer name is correct.
    if(!::_stricmp(msg -> name, "CheckPop")) {
        attempt_spawn(msg);
        start_timer();
    }

    return MS_CONTINUE;
}


/* =============================================================================
 *  TWTrapAIEcology Impmementation - private members
 */

void TWTrapAIEcology::start_timer(bool immediate)
{
    stop_timer(); // most of the time this is redundant, but be sure.
    update_timer = set_timed_message("CheckPop", immediate ? 100 : refresh, kSTM_OneShot);
}


void TWTrapAIEcology::stop_timer(void)
{
    if(update_timer) {
        cancel_timed_message(update_timer);
        update_timer.Clear();
    }
}


void TWTrapAIEcology::attempt_spawn(sScrMsg *msg)
{
    // Do nothing if there are already enough AIs spawned.
    if(!spawn_needed()) return;

    int archetype = select_archetype(msg);
    if(archetype) {
        int spawnpoint = select_spawnpoint(msg);

        if(spawnpoint) {
            spawn_ai(archetype, spawnpoint);

        } else if(debug_enabled()) {
            debug_printf(DL_WARNING, "Failed to locate an spawn point to spawn the AI at");
        }

    } else if(debug_enabled()) {
        debug_printf(DL_WARNING, "Failed to locate an archetype to spawn");
    }
}


bool TWTrapAIEcology::spawn_needed(void)
{
    linkset links;
    SService<ILinkSrv>      link_srv(g_pScriptManager);
	SService<ILinkToolsSrv> link_tools(g_pScriptManager);

    int count = 0;
    link_srv -> GetAll(links, link_tools -> LinkKindNamed("~Firer"), ObjId(), 0);
    // Ugh, there should be a better way to do this.
    for(; links.AnyLinksLeft(); ++count, links.NextLink()) {
        /* fnord */
    }

    // Less links than there can be spawned? If so, spawn is needed.
    return count < poplimit;
}


int TWTrapAIEcology::select_archetype(sScrMsg *msg)
{
    // Select the AI archetype to spawn an instance of. Note that this may return more than one
    // potential match, depending on the search term, but only the first archetype will be used
    std::vector<TargetObj> *archetype = get_target_objects(archetype_link.c_str(), msg);
    std::vector<TargetObj>::iterator it;

    int target = 0;
    // Traverse the list looking for the first matched archetype.
    for(it = archetype -> begin(); it != archetype -> end() && target >= 0; it++) {
        target = it -> obj_id;
    }

    delete archetype;

    // If an archetype was located, return it, otherwise 0 to indicate a failure.
    return(target < 0 ? target : 0);
}


int TWTrapAIEcology::select_spawnpoint(sScrMsg *msg)
{
    // Select the spawn point to use. This may return more than one potential match, in
    // which case only the first concrete object will be used.
    std::vector<TargetObj> *concrete = get_target_objects(spawnpoint_link.c_str(), msg);
    std::vector<TargetObj>::iterator it;

    int target = 0;
    // Traverse the list looking for the first matched concrete.
    for(it = concrete -> begin(); it != concrete -> end() && target <= 0; it++) {
        target = it -> obj_id;

        // We may need to reject the concrete if it is in view.
        if(target > 0) target = check_spawn_visibility(target);
    }

    delete concrete;

    // If a concrete object was located, return it, otherwise 0 to indicate a failure.
    return(target > 0 ? target : 0);
}


void TWTrapAIEcology::spawn_ai(int archetype, int spawnpoint)
{
    SService<IObjectSrv>    obj_srv(g_pScriptManager);
	SService<ILinkSrv>      link_srv(g_pScriptManager);
	SService<ILinkToolsSrv> link_tools(g_pScriptManager);
    SService<ISoundScrSrv>  snd_srv(g_pScriptManager);

    if(debug_enabled()) {
        std::string aname, sname;
        get_object_namestr(aname, archetype);
        get_object_namestr(sname, spawnpoint);
        debug_printf(DL_DEBUG, "Attempting to spawn an instance of %s at %s", aname.c_str(), sname.c_str());
    }

    object spawn;
    obj_srv -> BeginCreate(spawn, archetype);
    if(spawn) {
        cScrVec spawn_rot, spawn_pos;
        get_spawn_location(spawnpoint, spawn_rot, spawn_pos);

        // Move the AI into position
        obj_srv -> Teleport(spawn, spawn_pos, spawn_rot, 0);

        // Establish a link between the AI and the ecology controller
        link firer;
        link_srv -> Create(firer, link_tools -> LinkKindNamed("Firer"), spawn, ObjId());

        // Duplicate any AIWatch links on the spawn point
        copy_spawn_aiwatch(spawnpoint, spawn);

        obj_srv -> EndCreate(spawn);

        // Play a sound at the spawn point, maybe
        true_bool played;
        snd_srv -> PlayEnvSchema(played, spawnpoint, "Event Activate", spawnpoint, spawn, kEnvSoundAtObjLoc, kSoundNetNormal);

        // Send a TurnOn to the spawn point so it can do stuff and/or relay it.
        post_message(spawnpoint, "TurnOn");
    } else if(debug_enabled()) {
        std::string name;
        get_object_namestr(name, archetype);

        debug_printf(DL_WARNING, "BeginCreate failed to spawn instance of archetype %s", name.c_str());
    }
}


void TWTrapAIEcology::copy_spawn_aiwatch(object src, object dest)
{
    linkset links;
    SService<ILinkSrv>      link_srv(g_pScriptManager);
    SService<ILinkManager>  link_mgr(g_pScriptManager);
	SService<ILinkToolsSrv> link_tools(g_pScriptManager);

    link_srv -> GetAll(links, link_tools -> LinkKindNamed("AIWatchObj"), src, 0);
    for(; links.AnyLinksLeft(); links.NextLink()) {
		sLink link = links.Get();

        // Create a new link from the destination to the link dest of the correct flavour,
        // and copy any data it may have.
        long lcopy = link_mgr -> Add(dest, link.dest, link.flavor);
        if(lcopy) {
            void *data = links.Data();
            if(data) {
                link_mgr -> SetData(lcopy, data);
            }
        }
    }
}


int TWTrapAIEcology::check_spawn_visibility(int target)
{
    true_bool onscreen;
    SService<IObjectSrv> obj_srv(g_pScriptManager);

    // If spawns can happen in view, this function is a NOP basically.
    if(allow_visible_spawn) return target;

    // When debugging is on, explicitly check that the object does not have
    // Render Type: Not Rendered set
    if(debug_enabled()) {
        SService<IPropertySrv> prop_srv(g_pScriptManager);

        // Does it have a Render Type? If so, check what the render type is
        if(prop_srv -> Possessed(ObjId(), "RenderType")) {
            cMultiParm prop;
            prop_srv -> Get(prop, ObjId(), "RenderType", NULL);

            int mode = static_cast<int>(prop);
            // mode 0 is "Normal", mode 1 is "Unlit". Anything else will screw up vis check
            if(mode != 0 && mode != 2) {
                debug_printf(DL_WARNING, "Render Type is not 'Normal' or 'Unlit': visibility check will fail");
            }
        } else {
            debug_printf(DL_WARNING, "Attempt to check visibility with no Render Type property: visibility check will fail");
        }
    }

    // Otherwise, determine whether the target was rendered this frame
    obj_srv -> RenderedThisFrame(onscreen, target);

    // Only return the target id if it was not rendered.
    return onscreen ? 0 : target;
}


void TWTrapAIEcology::get_spawn_location(int spawnpoint, cScrVec& location, cScrVec& facing)
{
    SService<IObjectSrv> obj_srv(g_pScriptManager);

    obj_srv -> Position(location, spawnpoint);
    obj_srv -> Facing(facing, spawnpoint);

    // zero the pitch and bank, as having those non-zero can screw up AIs
    facing.y = facing.z = 0;

    // Does the spawn point have an offset set?
    char *design_note = GetObjectParams(spawnpoint);
    if(design_note) {
        cScrVec offset;
        get_scriptparam_floatvec(design_note, "SpawnOffset", offset);

        // offset will be filled with zeros if no offset has been set, so this is safe even if
        // no offset has actually been specified by the user.
        location += offset;

        g_pMalloc -> Free(design_note);
    }
}
