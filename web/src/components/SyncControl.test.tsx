import { fireEvent,render,screen,waitFor } from '@testing-library/react'
import { describe,expect,it,vi } from 'vitest'
import { SyncControl } from './SyncControl'
const headers=new Headers({'X-Trainlog-CSRF-Token':'c'.repeat(64)})
const reply=(value:unknown,status=200)=>({ok:status<300,status,headers,json:()=>Promise.resolve(value)})
describe('contrôle global de synchronisation',()=>{
  it('démarre une fois, confirme par polling et expose le brouillon sans le relabeller',async()=>{const fetch=vi.fn()
      .mockResolvedValueOnce(reply({phase:'idle',result:'idle'}))
      .mockResolvedValueOnce(reply({phase:'requested',result:'running'},202))
      .mockResolvedValueOnce(reply({phase:'completed',result:'completed',progress_revision:7,sessions_reconciled:1,finished_at:'2026-09-17T12:00:00Z',drafts:[{draft_id:'draft_1',state:'pending',session_type:'strength',occurrence_count:2}]}));vi.stubGlobal('fetch',fetch);const committed=vi.fn();render(<SyncControl onCommitted={committed}/>);await waitFor(()=>expect(screen.getByRole('button',{name:'Synchroniser'})).toBeEnabled());fireEvent.click(screen.getByRole('button',{name:'Synchroniser'}));await waitFor(()=>expect(screen.getByText('Synchronisation confirmée')).toBeInTheDocument(),{timeout:2500});fireEvent.click(screen.getByText('Synchronisation confirmée'));expect(screen.getByRole('region',{name:'Brouillons synchronisés'})).toHaveTextContent('pending');expect(committed).toHaveBeenCalledTimes(1);expect(fetch.mock.calls.filter(call=>String(call[0]).endsWith('/api/v1/sync'))).toHaveLength(1)})
})
