/*
 * Copyright (c) 2020 MariaDB Corporation Ab
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2026-08-08
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */
import Vue from 'vue'
import './pluginReg'
import 'styles/main.scss'
import i18n from '@share/plugins/i18n'
import vuetify from '@share/plugins/vuetify'
import App from './App.vue'
import router from 'router'
import commonComponents from '@share/components/common'

Object.keys(commonComponents).forEach(name => {
    Vue.component(name, commonComponents[name])
})
Vue.config.productionTip = false

new Vue({
    vuetify,
    router,
    i18n,
    render: h => h(App),
}).$mount('#app')
