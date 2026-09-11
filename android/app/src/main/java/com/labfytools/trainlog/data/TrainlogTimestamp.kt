package com.labfytools.trainlog.data

internal data class TrainlogTimestampKey(
    val utcSecond: Long,
    val fraction: String,
    val localYear: Int = 1,
    val localMonth: Int = 1,
    val localDay: Int = 1,
) : Comparable<TrainlogTimestampKey> {
    /* Calendar metadata is deliberately excluded: key identity remains the
     * same canonical-instant representation used before local grouping was
     * added. */
    override fun equals(other: Any?): Boolean =
        other is TrainlogTimestampKey && utcSecond == other.utcSecond && fraction == other.fraction

    override fun hashCode(): Int = 31 * utcSecond.hashCode() + fraction.hashCode()

    override fun compareTo(other: TrainlogTimestampKey): Int {
        utcSecond.compareTo(other.utcSecond).takeIf { it != 0 }?.let { return it }
        val count = maxOf(fraction.length, other.fraction.length)
        for (index in 0 until count) {
            val left = fraction.getOrElse(index) { '0' }
            val right = other.fraction.getOrElse(index) { '0' }
            if (left != right) return left.compareTo(right)
        }
        return 0
    }
}

internal object TrainlogTimestamp {
    /**
     * CONTRACT: parse the frozen ASCII timestamp grammar without inheriting a
     * platform ISO parser's syntax, offset bounds, or fractional precision.
     */
    fun parse(value: String): TrainlogTimestampKey? {
        if (value.length < 17 || value.any { it.code > 0x7f }) return null
        val year = digits(value, 0, 4) ?: return null
        val month = digits(value, 5, 2) ?: return null
        val day = digits(value, 8, 2) ?: return null
        val hour = digits(value, 11, 2) ?: return null
        val minute = digits(value, 14, 2) ?: return null
        if (value.getOrNull(4) != '-' || value.getOrNull(7) != '-' ||
            value.getOrNull(10) !in listOf('T', 't') || value.getOrNull(13) != ':' ||
            year !in 1..9999 || month !in 1..12 || day !in 1..daysInMonth(year, month) ||
            hour !in 0..23 || minute !in 0..59
        ) return null

        var at = 16
        var second = 0
        var fraction = ""
        if (value.getOrNull(at) == ':') {
            second = digits(value, at + 1, 2) ?: return null
            if (second !in 0..59) return null
            at += 3
            if (value.getOrNull(at) == '.') {
                val start = ++at
                while (value.getOrNull(at)?.isAsciiDigit() == true) at++
                if (at == start) return null
                fraction = value.substring(start, at)
            }
        }

        var offsetSeconds = 0L
        if (at + 1 == value.length && value[at] in listOf('Z', 'z')) {
            // UTC designator.
        } else {
            if (at + 6 != value.length || value.getOrNull(at) !in listOf('+', '-') ||
                value.getOrNull(at + 3) != ':'
            ) return null
            val offsetHour = digits(value, at + 1, 2) ?: return null
            val offsetMinute = digits(value, at + 4, 2) ?: return null
            if (offsetHour !in 0..23 || offsetMinute !in 0..59) return null
            offsetSeconds = (offsetHour * 3600L + offsetMinute * 60L) *
                if (value[at] == '+') 1L else -1L
        }
        val localSecond = dayNumber(year, month, day) * 86400L +
            hour * 3600L + minute * 60L + second
        return TrainlogTimestampKey(
            localSecond - offsetSeconds,
            fraction,
            year,
            month,
            day,
        )
    }

    /** Bytewise identity order shared with C's unsigned memcmp semantics. */
    fun compareIds(left: String, right: String): Int {
        val leftBytes = left.toByteArray(Charsets.UTF_8)
        val rightBytes = right.toByteArray(Charsets.UTF_8)
        for (index in 0 until minOf(leftBytes.size, rightBytes.size)) {
            val result = (leftBytes[index].toInt() and 0xff).compareTo(rightBytes[index].toInt() and 0xff)
            if (result != 0) return result
        }
        return leftBytes.size.compareTo(rightBytes.size)
    }

    private fun digits(value: String, start: Int, count: Int): Int? {
        if (start < 0 || start + count > value.length) return null
        var result = 0
        repeat(count) { offset ->
            val character = value[start + offset]
            if (!character.isAsciiDigit()) return null
            result = result * 10 + (character - '0')
        }
        return result
    }

    private fun Char.isAsciiDigit(): Boolean = this in '0'..'9'
    private fun leap(year: Int): Boolean = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)
    private fun daysInMonth(year: Int, month: Int): Int = when (month) {
        2 -> if (leap(year)) 29 else 28
        4, 6, 9, 11 -> 30
        else -> 31
    }

    private fun dayNumber(year: Int, month: Int, day: Int): Long {
        val prior = year - 1
        var days = prior * 365L + prior / 4 - prior / 100 + prior / 400
        for (current in 1 until month) days += daysInMonth(year, current)
        return days + day - 1
    }
}
