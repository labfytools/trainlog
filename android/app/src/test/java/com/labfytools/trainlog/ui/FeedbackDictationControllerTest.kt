package com.labfytools.trainlog.ui

import org.junit.Assert.*
import org.junit.Test

class FeedbackDictationControllerTest {
    private class Fake(override var available:Boolean=true):SpeechRecognitionAdapter{
        var listener:SpeechRecognitionAdapter.Listener?=null;var stopped=false;var destroyed=false
        override fun start(languageTag:String,listener:SpeechRecognitionAdapter.Listener){assertEquals("fr-FR",languageTag);this.listener=listener}
        override fun stop(){stopped=true};override fun destroy(){destroyed=true}
    }
    @Test fun `partial final correction resume append stop and save`() {
        val fake=Fake();val c=FeedbackDictationController(fake)
        c.start(true);assertEquals(DictationPhase.Listening,c.state.phase)
        fake.listener!!.onPartial("premier");assertEquals("premier",c.state.visibleText)
        fake.listener!!.onPartial("premier segment");assertEquals("premier segment",c.state.visibleText)
        fake.listener!!.onFinal("premier segment");assertEquals(DictationPhase.Editing,c.state.phase)
        c.edit("segment corrigé");c.start(true);fake.listener!!.onPartial("suite");c.stop()
        assertEquals("segment corrigé suite",c.state.visibleText)
        assertEquals("segment corrigé suite",c.beginSaving())
        assertEquals(DictationPhase.Saving,c.state.phase)
    }
    @Test fun `long multiline partial survives immediate stop and resume appends`() {
        val fake=Fake();val c=FeedbackDictationController(fake)
        c.edit("ligne une\nligne deux corrigée")
        c.start(true)
        val partial="ligne trois très longue qui doit rester intégralement visible après l'arrêt"
        fake.listener!!.onPartial(partial)
        c.stop()
        assertTrue(fake.stopped)
        assertEquals("ligne une\nligne deux corrigée $partial",c.state.visibleText)
        assertEquals(DictationPhase.Editing,c.state.phase)
        c.start(true);fake.listener!!.onFinal("ligne quatre")
        assertEquals("ligne une\nligne deux corrigée $partial ligne quatre",c.state.visibleText)
    }
    @Test fun `errors preserve text and manual entry remains usable`() {
        val fake=Fake();val c=FeedbackDictationController(fake);c.edit("conservé")
        c.start(false);assertEquals("conservé",c.state.visibleText);c.edit("manuel")
        fake.available=false;c.start(true);assertEquals("manuel",c.state.visibleText)
        fake.available=true;c.start(true);fake.listener!!.onPartial("partiel");fake.listener!!.onError("erreur")
        assertEquals("manuel partiel",c.state.visibleText);c.cancel();assertEquals("",c.state.visibleText)
    }
    @Test fun `blank save forbidden and lifecycle destroys recognizer`() {
        val fake=Fake();val c=FeedbackDictationController(fake)
        assertNull(c.beginSaving());c.destroy();assertTrue(fake.destroyed)
    }
}
