import { beforeEach,describe,expect,it,vi } from 'vitest'
import { fetchSyncStatus,startSync,syncIsActive } from './sync'
const token='b'.repeat(64)
function response(value:unknown,status=200){return {ok:status>=200&&status<300,status,headers:new Headers({'X-Trainlog-CSRF-Token':token}),json:()=>Promise.resolve(value)}}
describe('sync API',()=>{beforeEach(()=>vi.restoreAllMocks())
  it('obtient passivement le statut et utilise le jeton pour la mutation',async()=>{const fetch=vi.fn().mockResolvedValueOnce(response({phase:'idle',result:'disabled'})).mockResolvedValueOnce(response({phase:'requested',result:'running'},202));vi.stubGlobal('fetch',fetch);await fetchSyncStatus();await startSync('sy_11111111-1111-4111-8111-111111111111');expect(fetch).toHaveBeenCalledTimes(2);const init=fetch.mock.calls[1][1];expect(init.method).toBe('POST');expect(init.headers['X-Trainlog-CSRF-Token']).toBe(token)})
  it('rejette un statut inconnu et classe les phases actives',async()=>{vi.stubGlobal('fetch',vi.fn().mockResolvedValue(response({phase:'invented',result:'ok'})));await expect(fetchSyncStatus()).rejects.toThrow();expect(syncIsActive('waiting_acknowledgement')).toBe(true);expect(syncIsActive('completed')).toBe(false)})
})
