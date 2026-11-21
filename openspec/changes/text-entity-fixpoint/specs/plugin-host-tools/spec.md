# Spec Delta

## ADDED Requirements

### Requirement: Display text cleanup helper

Plugins SHALL call `host.text.clean(s)` to turn scraped HTML text into display-ready
text. The host SHALL decode HTML entities to a fixed point (at most 4 passes), trim the
result, and expose the same helper in all three runtimes over the shared core.
Multiply-encoded forms and the lenient numeric fallback are defined by the scenarios
below.

#### Scenario: Single-encoded entity
- **WHEN** a plugin calls `clean("So What&#39;s Wrong")`
- **THEN** the result is `So What's Wrong`

#### Scenario: Double and triple encoding
- **WHEN** a plugin calls `clean("A &amp;#39;s")` or `clean("A &amp;039;s")`
- **THEN** both results are `A's`

#### Scenario: Hex numeric reference
- **WHEN** a plugin calls `clean("A &#x27;s")`
- **THEN** the result is `A's`

#### Scenario: Plain text and real ampersand are preserved
- **WHEN** a plugin calls `clean("Tom & Jerry")` or `clean("https://x/api?a=1&b=2")`
- **THEN** both strings are returned unchanged

#### Scenario: Malformed numeric reference
- **WHEN** a numeric reference resolves to an invalid codepoint (0, surrogate, or above U+10FFFF)
- **THEN** the literal reference text is preserved and no error is raised

### Requirement: Bounded decoding cost

The host SHALL skip entity decoding entirely when the input contains no `&` character,
and SHALL cap decoding at 4 passes so pathological nested encodings terminate. Decoding
SHALL use the existing lexbor HTML parser for entity resolution and the existing re2 /
utf8proc libraries for the lenient numeric-reference fallback; no new dependency SHALL
be introduced.

#### Scenario: No entity marker
- **WHEN** a plugin calls `clean` on plain ASCII text
- **THEN** no HTML parse is performed and the trimmed input is returned

#### Scenario: Pathological nesting
- **WHEN** a plugin calls `clean` on a string with more than 4 layers of `&amp;`
- **THEN** the call terminates within 4 passes without throwing
