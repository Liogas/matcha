ALTER TABLE matcha_profile_photos
	ADD COLUMN content_type TEXT NOT NULL DEFAULT 'image/webp',
	ADD COLUMN file_size BIGINT NOT NULL DEFAULT 0,
	ADD COLUMN width INTEGER NOT NULL DEFAULT 0,
	ADD COLUMN height INTEGER NOT NULL DEFAULT 0,
	ADD COLUMN file_hash CHAR(64) NOT NULL DEFAULT
		'0000000000000000000000000000000000000000000000000000000000000000';

ALTER TABLE matcha_profile_photos
	ADD CONSTRAINT matcha_profile_photos_content_type
		CHECK (content_type = 'image/webp'),
	ADD CONSTRAINT matcha_profile_photos_file_size
		CHECK (file_size > 0),
	ADD CONSTRAINT matcha_profile_photos_dimensions
		CHECK (width > 0 AND height > 0),
	ADD CONSTRAINT matcha_profile_photos_file_hash
		CHECK (file_hash ~ '^[a-f0-9]{64}$');

CREATE INDEX matcha_profile_photos_profile_position_idx
	ON matcha_profile_photos (profile_account_id, position);
