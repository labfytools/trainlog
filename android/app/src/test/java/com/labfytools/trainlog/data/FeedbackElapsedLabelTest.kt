package com.labfytools.trainlog.data
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
class FeedbackElapsedLabelTest {
 @Test fun `whole hours floor and never switch to days`() {
  val end="2026-09-01T10:00:00+02:00"
  listOf(0 to "2026-09-01T10:59:59+02:00",8 to "2026-09-01T18:00:00+02:00",23 to "2026-09-02T09:59:59+02:00",47 to "2026-09-03T09:00:00+02:00",72 to "2026-09-04T10:00:00+02:00").forEach{(h,t)->assertEquals("H+$h",sessionFollowUpElapsedLabel(end,t))}
 }
 @Test fun `unknown followup anchor is explicit`()=assertEquals("H+?",sessionFollowUpElapsedLabel(null,"2026-09-01T18:00:00+02:00"))
 @Test fun `exercise feedback distinguishes session time after time and unknown`() {
  val end="2026-09-01T10:00:00+02:00"
  assertEquals("Pendant la séance",exerciseFeedbackElapsedLabel(end,"2026-09-01T09:20:00+02:00"))
  assertEquals("Pendant la séance",exerciseFeedbackElapsedLabel(end,"2026-09-01T09:59:59+02:00"))
  assertEquals("H+0",exerciseFeedbackElapsedLabel(end,end))
  assertEquals("H+8",exerciseFeedbackElapsedLabel(end,"2026-09-01T18:00:00+02:00"))
  assertEquals("Ressenti",exerciseFeedbackElapsedLabel(null,"2026-09-01T09:20:00+02:00"))
 }
 @Test fun `helpers never render negative H plus`() {
  val labels=listOf(exerciseFeedbackElapsedLabel("2026-09-01T10:00:00+02:00","2026-09-01T09:00:00+02:00"),sessionFollowUpElapsedLabel("2026-09-01T10:00:00+02:00","2026-09-01T09:00:00+02:00"))
  assertTrue(labels.none{it.startsWith("H+-")})
 }
}
