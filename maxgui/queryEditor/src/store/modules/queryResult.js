/*
 * Copyright (c) 2020 MariaDB Corporation Ab
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2026-08-25
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */
import queryHelper from './queryHelper'

const statesToBeSynced = queryHelper.syncStateCreator('queryResult')
const memStates = queryHelper.memStateCreator('queryResult')
export default {
    namespaced: true,
    state: {
        ...memStates,
        ...statesToBeSynced,
        is_max_rows_valid: true,
    },
    mutations: {
        ...queryHelper.memStatesMutationCreator(memStates),
        ...queryHelper.syncedStateMutationsCreator({
            statesToBeSynced,
            persistedArrayPath: 'querySession.query_sessions',
        }),
        SET_IS_MAX_ROWS_VALID(state, payload) {
            state.is_max_rows_valid = payload
        },
    },
    actions: {
        /**
         * @param {String} tblId - Table id (database_name.table_name).
         */
        async fetchPrvw({ rootState, commit, dispatch, rootGetters }, { tblId, prvwMode }) {
            const active_sql_conn = rootState.queryConn.active_sql_conn
            const active_session_id = rootGetters['querySession/getActiveSessionId']
            const request_sent_time = new Date().valueOf()
            try {
                commit(`PATCH_${prvwMode}_MAP`, {
                    id: active_session_id,
                    payload: {
                        request_sent_time,
                        total_duration: 0,
                        is_loading: true,
                    },
                })
                let sql, queryName
                const escapedTblId = this.vue.$helpers.escapeIdentifiers(tblId)
                switch (prvwMode) {
                    case rootState.queryEditorConfig.config.SQL_QUERY_MODES.PRVW_DATA:
                        sql = `SELECT * FROM ${escapedTblId} LIMIT 1000;`
                        queryName = `Preview ${escapedTblId} data`
                        break
                    case rootState.queryEditorConfig.config.SQL_QUERY_MODES.PRVW_DATA_DETAILS:
                        sql = `DESCRIBE ${escapedTblId};`
                        queryName = `View ${escapedTblId} details`
                        break
                }

                let res = await this.vue.$queryHttp.post(`/sql/${active_sql_conn.id}/queries`, {
                    sql,
                    max_rows: rootState.queryPersisted.query_row_limit,
                })
                const now = new Date().valueOf()
                const total_duration = ((now - request_sent_time) / 1000).toFixed(4)
                commit(`PATCH_${prvwMode}_MAP`, {
                    id: active_session_id,
                    payload: {
                        data: Object.freeze(res.data.data),
                        total_duration: parseFloat(total_duration),
                        is_loading: false,
                    },
                })
                dispatch(
                    'queryPersisted/pushQueryLog',
                    {
                        startTime: now,
                        name: queryName,
                        sql,
                        res,
                        connection_name: active_sql_conn.name,
                        queryType: rootState.queryEditorConfig.config.QUERY_LOG_TYPES.ACTION_LOGS,
                    },
                    { root: true }
                )
            } catch (e) {
                commit(`PATCH_${prvwMode}_MAP`, {
                    id: active_session_id,
                    payload: { is_loading: false },
                })
                this.vue.$logger(`store-queryResult-fetchPrvw`).error(e)
            }
        },
        /**
         * @param {String} query - SQL query string
         */
        async fetchQueryResult({ commit, dispatch, rootState, rootGetters }, query) {
            const active_sql_conn = rootState.queryConn.active_sql_conn
            const request_sent_time = new Date().valueOf()
            const active_session_id = rootGetters['querySession/getActiveSessionId']
            try {
                commit('PATCH_QUERY_RESULTS_MAP', {
                    id: active_session_id,
                    payload: {
                        data: {},
                        request_sent_time,
                        total_duration: 0,
                        is_loading: true,
                    },
                })

                /**
                 * dispatch cloneConn before running the user's query to prevent concurrent
                 * querying of the same connection.
                 * This "BACKGROUND" connection must be disconnected after finnish the user's query.
                 * i.e. dispatch disconnectBgConn
                 */
                await dispatch(
                    'queryConn/cloneConn',
                    {
                        conn_to_be_cloned: active_sql_conn,
                        binding_type:
                            rootState.queryEditorConfig.config.QUERY_CONN_BINDING_TYPES.BACKGROUND,
                        session_id_fk: active_session_id,
                    },
                    { root: true }
                )

                let res = await this.vue.$queryHttp.post(`/sql/${active_sql_conn.id}/queries`, {
                    sql: query,
                    max_rows: rootState.queryPersisted.query_row_limit,
                })
                const now = new Date().valueOf()
                const total_duration = ((now - request_sent_time) / 1000).toFixed(4)

                commit('PATCH_QUERY_RESULTS_MAP', {
                    id: active_session_id,
                    payload: {
                        data: Object.freeze(res.data.data),
                        total_duration: parseFloat(total_duration),
                        is_loading: false,
                    },
                })

                const USE_REG = /(use|drop database)\s/i
                if (query.match(USE_REG))
                    await dispatch('queryConn/updateActiveDb', {}, { root: true })
                dispatch(
                    'queryPersisted/pushQueryLog',
                    {
                        startTime: now,
                        sql: query,
                        res,
                        connection_name: active_sql_conn.name,
                        queryType: rootState.queryEditorConfig.config.QUERY_LOG_TYPES.USER_LOGS,
                    },
                    { root: true }
                )
            } catch (e) {
                commit('PATCH_QUERY_RESULTS_MAP', {
                    id: active_session_id,
                    payload: { is_loading: false },
                })
                this.vue.$logger(`store-queryResult-fetchQueryResult`).error(e)
            }
        },
        async stopQuery({ commit, rootGetters, rootState }) {
            const active_sql_conn = rootState.queryConn.active_sql_conn
            const active_session_id = rootGetters['querySession/getActiveSessionId']
            try {
                commit('PATCH_IS_STOPPING_QUERY_MAP', {
                    id: active_session_id,
                    payload: { value: true },
                })
                const bgConn = rootGetters['queryConn/getBgConn']({
                    session_id_fk: active_session_id,
                })
                const {
                    data: { data: { attributes: { results = [] } = {} } = {} } = {},
                } = await this.vue.$queryHttp.post(`/sql/${bgConn.id}/queries`, {
                    sql: `KILL QUERY ${active_sql_conn.attributes.thread_id}`,
                })
                if (results.length && results[0].errno)
                    commit(
                        'appNotifier/SET_SNACK_BAR_MESSAGE',
                        {
                            text: [
                                'Failed to stop the query',
                                ...Object.keys(results[0]).map(key => `${key}: ${results[0][key]}`),
                            ],
                            type: 'error',
                        },
                        { root: true }
                    )
            } catch (e) {
                this.vue.$logger(`store-queryResult-stopQuery`).error(e)
            }
            commit('PATCH_IS_STOPPING_QUERY_MAP', {
                id: active_session_id,
                payload: { value: false },
            })
        },
        /**
         * This action clears prvw_data and prvw_data_details to empty object.
         * Call this action when user selects option in the sidebar.
         * This ensure sub-tabs in Data Preview tab are generated with fresh data
         */
        clearDataPreview({ commit, rootGetters }) {
            const active_session_id = rootGetters['querySession/getActiveSessionId']
            commit(`PATCH_PRVW_DATA_MAP`, { id: active_session_id })
            commit(`PATCH_PRVW_DATA_DETAILS_MAP`, { id: active_session_id })
        },
    },
    getters: {
        getUserQueryRes: (state, getters, rootState, rootGetters) =>
            state.query_results_map[rootGetters['querySession/getActiveSessionId']] || {},
        getLoadingQueryResultBySessionId: state => {
            return session_id => {
                const { is_loading = false } = state.query_results_map[session_id] || {}
                return is_loading
            }
        },
        isWkeLoadingQueryResult: (state, getters, rootState, rootGetters) => {
            return wke_id => {
                const sessionIds = rootGetters['querySession/getSessionsByWkeId'](wke_id).map(
                    s => s.id
                )
                let isLoading = false
                for (const key of Object.keys(state.query_results_map)) {
                    if (sessionIds.includes(key)) {
                        const { is_loading = false } = state.query_results_map[key] || {}
                        if (is_loading) {
                            isLoading = true
                            break
                        }
                    }
                }
                return isLoading
            }
        },
        getIsStoppingQueryBySessionId: state => {
            return session_id => {
                const { value = false } = state.is_stopping_query_map[session_id] || {}
                return value
            }
        },
        getPrvwData: (state, getters, rootState, rootGetters) => mode => {
            let map = state[`${mode.toLowerCase()}_map`]
            if (map) return map[rootGetters['querySession/getActiveSessionId']] || {}
            return {}
        },
        getIsRunBtnDisabledBySessionId: (state, getters, rootState, rootGetters) => {
            return id => {
                const session = rootState.querySession.query_sessions.find(s => s.id === id)
                if (!session) return true
                return (
                    !session.query_txt ||
                    !session.active_sql_conn.id ||
                    (rootGetters['queryConn/getIsConnBusyBySessionId'](session.id) &&
                        getters.getLoadingQueryResultBySessionId(session.id)) ||
                    !state.is_max_rows_valid
                )
            }
        },
        getIsVisBtnDisabledBySessionId: (state, getters, rootState, rootGetters) => {
            return id => {
                const session = rootState.querySession.query_sessions.find(s => s.id === id)
                if (!session) return true
                return (
                    !session.active_sql_conn.id ||
                    (rootGetters['queryConn/getIsConnBusyBySessionId'](session.id) &&
                        getters.getLoadingQueryResultBySessionId(session.id))
                )
            }
        },
        getChartResultSets: (state, getters, rootState, rootGetters) => ({ scope }) => {
            let resSets = []
            // user query result data
            const userQueryResults = scope.$helpers.stringifyClone(
                scope.$typy(getters.getUserQueryRes, 'data.attributes.results').safeArray
            )
            let resSetCount = 0
            for (const res of userQueryResults) {
                if (res.data) {
                    ++resSetCount
                    resSets.push({ id: `RESULT SET ${resSetCount}`, ...res })
                }
            }
            // preview data
            const {
                PRVW_DATA,
                PRVW_DATA_DETAILS,
            } = rootState.queryEditorConfig.config.SQL_QUERY_MODES
            const prvwModes = [PRVW_DATA, PRVW_DATA_DETAILS]
            for (const mode of prvwModes) {
                const data = scope.$helpers.stringifyClone(
                    scope.$typy(getters.getPrvwData(mode), 'data.attributes.results[0]')
                        .safeObjectOrEmpty
                )
                if (!scope.$typy(data).isEmptyObject) {
                    let resName = ''
                    switch (mode) {
                        case PRVW_DATA:
                            resName = scope.$mxs_t('previewData')
                            break
                        case PRVW_DATA_DETAILS:
                            resName = scope.$mxs_t('viewDetails')
                            break
                    }
                    resSets.push({
                        id: `${resName} of ${rootGetters['schemaSidebar/getActivePrvwTblNode'].id}`,
                        ...data,
                    })
                }
            }
            return resSets
        },
    },
}
