# Spec Delta: manga-detail-page

## ADDED Requirements

### Requirement: Icon-only controls expose accessible names
Every icon-only or image-only interactive control rendered by the detail page and the
library grid SHALL carry an accessible name, including the per-card plugin control, the
status filter controls, the dim toggle and the reader pager buttons. A control whose only
content is an icon SHALL NOT rely on a `title` attribute alone.

#### Scenario: Icon-only control has a name
- **WHEN** the detail page or the library grid renders a control whose content is only an icon
- **THEN** that control exposes an accessible name

#### Scenario: Named control still renders
- **WHEN** an icon-only control gains an accessible name
- **THEN** its visible glyph and layout are unchanged

### Requirement: Focus indication stays visible
A control that receives keyboard focus on the templates' dark surfaces SHALL render a
visible focus indicator. Removing the outline without supplying a visible replacement SHALL
NOT be treated as meeting this requirement.

#### Scenario: Focus stays visible
- **WHEN** a control receives keyboard focus on a dark surface
- **THEN** a visible focus indicator is rendered
