import { beforeEach,describe,expect,it,vi } from 'vitest'
import { fetchSyncStatus, newRequestId, startSync, syncIsActive } from './sync'
const token='b'.repeat(64)
function response(value:unknown,status=200,csrf=token){return {ok:status>=200&&status<300,status,headers:new Headers({'X-Trainlog-CSRF-Token':csrf}),json:()=>Promise.resolve(value)}}
describe('sync API',()=>{beforeEach(()=>vi.restoreAllMocks())
  it('obtient passivement le statut et utilise le jeton pour la mutation',async()=>{const fetch=vi.fn().mockResolvedValueOnce(response({phase:'idle',result:'disabled'})).mockResolvedValueOnce(response({phase:'requested',result:'running'},202));vi.stubGlobal('fetch',fetch);await fetchSyncStatus();await startSync('sy_11111111-1111-4111-8111-111111111111');expect(fetch).toHaveBeenCalledTimes(2);const init=fetch.mock.calls[1][1];expect(init.method).toBe('POST');expect(init.headers['X-Trainlog-CSRF-Token']).toBe(token)})
  it('rafraîchit une fois un jeton CSRF devenu caduc après redémarrage du serveur',async()=>{
    const oldToken='a'.repeat(64)
    const freshToken='d'.repeat(64)
    const requestId='sy_22222222-2222-4222-8222-222222222222'
    const fetch=vi.fn()
      .mockResolvedValueOnce(response({phase:'failed',result:'failed'},200,oldToken))
      .mockResolvedValueOnce(response({error:'mutation_forbidden'},403,oldToken))
      .mockResolvedValueOnce(response({phase:'failed',result:'failed'},200,freshToken))
      .mockResolvedValueOnce(response({phase:'requested',result:'running'},202,freshToken))
    vi.stubGlobal('fetch',fetch)
    await fetchSyncStatus()
    await expect(startSync(requestId)).resolves.toMatchObject({phase:'requested'})
    expect(fetch).toHaveBeenCalledTimes(4)
    expect(fetch.mock.calls[1][1].headers['X-Trainlog-CSRF-Token']).toBe(oldToken)
    expect(fetch.mock.calls[2][0]).toBe('/api/v1/sync/status')
    expect(fetch.mock.calls[3][1].headers['X-Trainlog-CSRF-Token']).toBe(freshToken)
    expect(fetch.mock.calls[1][1].body).toBe(fetch.mock.calls[3][1].body)
  })
  it('rejette un statut inconnu et classe les phases actives',async()=>{vi.stubGlobal('fetch',vi.fn().mockResolvedValue(response({phase:'invented',result:'ok'})));await expect(fetchSyncStatus()).rejects.toThrow();expect(syncIsActive('waiting_acknowledgement')).toBe(true);expect(syncIsActive('completed')).toBe(false)})
  it('crée un UUIDv4 sans dépendre de randomUUID', () => {
    vi.stubGlobal('crypto', {
      getRandomValues: (bytes: Uint8Array) => {
        bytes.set(Array.from({ length: 16 }, (_, index) => index))
        return bytes
      },
    })
    expect(newRequestId()).toBe('sy_00010203-0405-4607-8809-0a0b0c0d0e0f')
  })
})
