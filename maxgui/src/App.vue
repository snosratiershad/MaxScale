<template>
    <v-app>
        <overlay />
        <component :is="$route.meta.layout"> </component>
        <mxs-dlg
            v-model="updateExists"
            :onSave="confirmUpdate"
            :title="`${$mxs_t('newUpdateAvailable')}`"
            saveText="update"
            isForceAccept
        />
        <mxs-truncate-tooltip />
    </v-app>
</template>

<script>
/*
 * Copyright (c) 2020 MariaDB Corporation Ab
 * Copyright (c) 2023 MariaDB plc, Finnish Branch
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2028-01-30
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 *  Public License.
 */
import '@rootSrc/styles/main.scss'
import store from '@rootSrc/store'
import AppLayout from '@rootSrc/layouts/AppLayout'
import NoLayout from '@rootSrc/layouts/NoLayout'
import Overlay from '@share/components/overlay'
import { mapState, mapMutations, mapActions } from 'vuex'

export default {
    store,
    components: {
        Overlay,
        AppLayout,
        NoLayout,
    },

    computed: {
        ...mapState(['update_availability']),
        ...mapState({ is_session_alive: state => state.mxsApp.is_session_alive }),
        updateExists: function() {
            return this.update_availability
        },
    },
    watch: {
        $route: function() {
            // Clear global search state when route changes
            this.SET_SEARCH_KEYWORD('')
        },
        async is_session_alive(v) {
            if (!v) await this.logout()
        },
    },
    mounted() {
        let overlay = document.getElementById('global-overlay')
        if (overlay) {
            overlay.style.display = 'none'
        }
    },
    async created() {
        this.$logger.info(this.$store.state.app_config.asciiLogo)
        this.$logger.info(`Loaded Version: ${process.env.VUE_APP_VERSION}`)
        // Check if user is authenticated
        await this.authCheck()
    },
    methods: {
        ...mapMutations(['SET_UPDATE_AVAILABILITY', 'SET_SEARCH_KEYWORD']),
        ...mapActions({ authCheck: 'user/authCheck', logout: 'user/logout' }),
        confirmUpdate() {
            this.SET_UPDATE_AVAILABILITY(false)
            window.location.reload()
            this.$logger.info('App is updated')
        },
    },
}
</script>
