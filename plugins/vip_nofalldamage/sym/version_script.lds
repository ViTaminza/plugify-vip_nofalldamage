{
    global:
        Plugify_*;

        /* s2sdk binding slots (defined by s2sdk_bindings, filled by the plugify module) */
        __s2sdk_*;

        /* vip binding slots (defined by the generated consumer bindings, filled by the plugify
           module from the vip plugin's exported methods) */
        __vip_*;

        /* polyhook binding slots (defined by polyhook_bindings, filled by the plugify module) */
        __polyhook_*;

        /* hudmenu binding slots (defined by hudmenu_bindings via pisexcompat) */
        __hudmenu_*;

        /* No exported methods: vip_nofalldamage exposes nothing to other plugins. */

    local: *;
};
