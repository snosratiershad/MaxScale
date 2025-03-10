<template>
    <v-icon
        :class="iconClass"
        :size="icon === emptyIcon ? 18 : size"
        :style="icon === emptyIcon ? { margin: '0 -2px' } : ''"
        :color="color"
    >
        {{ icon }}
    </v-icon>
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
 * Public License.
 */
import { mapState } from 'vuex'
export default {
    name: 'icon-sprite-sheet',
    props: {
        frame: {
            type: [Number, String],
            required: true,
        },
        size: [Number, String],
        color: String,
        frames: [Array, Object],
        colorClasses: [Array, Object],
    },
    data() {
        return {
            emptyIcon: 'mdi-bug', // icon to be shown if there is a missing "frame"
        }
    },
    computed: {
        ...mapState({ ICON_SHEETS: state => state.app_config.ICON_SHEETS }),
        sheet() {
            const name = this.$slots.default ? this.$slots.default[0].text.trim() : ''
            const sheet = this.ICON_SHEETS[name] || {}
            const frames = this.frames || sheet.frames || []
            const colorClasses = this.colorClasses || sheet.colorClasses || []

            return { frames, colorClasses }
        },
        icon() {
            if (!this.sheet.frames[this.frame]) return this.emptyIcon

            return this.sheet.frames[this.frame]
        },
        iconClass() {
            // NOTE: if the mxs-color-helper is set trough color: property mxs-color-helper classes are ignored
            if (this.color || !this.sheet.colorClasses[this.frame]) return ''

            return 'mxs-color-helper ' + this.sheet.colorClasses[this.frame]
        },
    },
}
</script>
