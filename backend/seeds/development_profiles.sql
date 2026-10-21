-- Development-only fixtures. Run this file explicitly against the development database.
-- All seeded accounts use: MatchaTest123!

INSERT INTO auth_accounts
	(email, password_hash, email_verified_at)
VALUES
	('alice.bernard@example.test', '$argon2id$v=19$m=65536,t=2,p=1$O9Fx5K0RO5oQWFu46MTvow$k2l5zDE44WVxpeAXuNhChvQtc48M4v57vLb74Jtfqck', NOW()),
	('bruno.martin@example.test', '$argon2id$v=19$m=65536,t=2,p=1$O9Fx5K0RO5oQWFu46MTvow$k2l5zDE44WVxpeAXuNhChvQtc48M4v57vLb74Jtfqck', NOW()),
	('camille.robert@example.test', '$argon2id$v=19$m=65536,t=2,p=1$O9Fx5K0RO5oQWFu46MTvow$k2l5zDE44WVxpeAXuNhChvQtc48M4v57vLb74Jtfqck', NOW()),
	('diane.petit@example.test', '$argon2id$v=19$m=65536,t=2,p=1$O9Fx5K0RO5oQWFu46MTvow$k2l5zDE44WVxpeAXuNhChvQtc48M4v57vLb74Jtfqck', NOW()),
	('elias.durand@example.test', '$argon2id$v=19$m=65536,t=2,p=1$O9Fx5K0RO5oQWFu46MTvow$k2l5zDE44WVxpeAXuNhChvQtc48M4v57vLb74Jtfqck', NOW()),
	('fatou.diallo@example.test', '$argon2id$v=19$m=65536,t=2,p=1$O9Fx5K0RO5oQWFu46MTvow$k2l5zDE44WVxpeAXuNhChvQtc48M4v57vLb74Jtfqck', NOW()),
	('gabriel.lemoine@example.test', '$argon2id$v=19$m=65536,t=2,p=1$O9Fx5K0RO5oQWFu46MTvow$k2l5zDE44WVxpeAXuNhChvQtc48M4v57vLb74Jtfqck', NOW()),
	('ines.moreau@example.test', '$argon2id$v=19$m=65536,t=2,p=1$O9Fx5K0RO5oQWFu46MTvow$k2l5zDE44WVxpeAXuNhChvQtc48M4v57vLb74Jtfqck', NOW()),
	('julien.faure@example.test', '$argon2id$v=19$m=65536,t=2,p=1$O9Fx5K0RO5oQWFu46MTvow$k2l5zDE44WVxpeAXuNhChvQtc48M4v57vLb74Jtfqck', NOW()),
	('leila.girard@example.test', '$argon2id$v=19$m=65536,t=2,p=1$O9Fx5K0RO5oQWFu46MTvow$k2l5zDE44WVxpeAXuNhChvQtc48M4v57vLb74Jtfqck', NOW())
ON CONFLICT (LOWER(email)) DO UPDATE SET
	password_hash = EXCLUDED.password_hash,
	email_verified_at = COALESCE(auth_accounts.email_verified_at,
		EXCLUDED.email_verified_at);

INSERT INTO matcha_profiles
	(account_id, username, first_name, last_name, date_of_birth, gender,
	 sexual_orientation, biography, city, completed_at)
SELECT a.id, fixture.username, fixture.first_name, fixture.last_name,
	fixture.date_of_birth::date, fixture.gender, fixture.sexual_orientation,
	fixture.biography, fixture.city, NOW()
FROM (
	VALUES
		('alice.bernard@example.test', 'aliceb', 'Alice', 'Bernard', '1996-04-18', 'woman', 'bisexual', 'Photographie, cafés tranquilles et escapades improvisées.', 'Paris'),
		('bruno.martin@example.test', 'brunom', 'Bruno', 'Martin', '1992-09-03', 'man', 'heterosexual', 'Je cuisine beaucoup trop et je cherche quelqu’un pour partager.', 'Lyon'),
		('camille.robert@example.test', 'camiller', 'Camille', 'Robert', '1998-01-27', 'non_binary', 'pansexual', 'Design, concerts intimistes et longues discussions.', 'Bordeaux'),
		('diane.petit@example.test', 'dianep', 'Diane', 'Petit', '1994-12-11', 'woman', 'heterosexual', 'Curieuse, sportive et toujours partante pour découvrir une adresse.', 'Nantes'),
		('elias.durand@example.test', 'eliasd', 'Elias', 'Durand', '1990-06-22', 'man', 'homosexual', 'Architecture, randonnée et bons films du dimanche.', 'Toulouse'),
		('fatou.diallo@example.test', 'fatoud', 'Fatou', 'Diallo', '1997-08-09', 'woman', 'bisexual', 'Danse, littérature et énergie positive au quotidien.', 'Montpellier'),
		('gabriel.lemoine@example.test', 'gabriell', 'Gabriel', 'Lemoine', '1993-03-15', 'man', 'bisexual', 'Musique, vélo et cuisine végétale.', 'Strasbourg'),
		('ines.moreau@example.test', 'inesm', 'Ines', 'Moreau', '1999-11-02', 'woman', 'pansexual', 'Je collectionne les plantes et les histoires de voyage.', 'Lille'),
		('julien.faure@example.test', 'julienf', 'Julien', 'Faure', '1988-07-30', 'man', 'heterosexual', 'Entrepreneur le jour, passionné de jeux de société le soir.', 'Rennes'),
		('leila.girard@example.test', 'leilag', 'Leila', 'Girard', '1995-02-06', 'woman', 'homosexual', 'Mer, cuisine méditerranéenne et week-ends sans programme.', 'Nice')
) AS fixture(email, username, first_name, last_name, date_of_birth, gender,
	sexual_orientation, biography, city)
JOIN auth_accounts a ON a.email = fixture.email
ON CONFLICT (account_id) DO UPDATE SET
	date_of_birth = EXCLUDED.date_of_birth,
	gender = EXCLUDED.gender,
	sexual_orientation = EXCLUDED.sexual_orientation,
	biography = EXCLUDED.biography,
	city = EXCLUDED.city,
	completed_at = EXCLUDED.completed_at,
	updated_at = NOW();

INSERT INTO matcha_profile_photos
	(profile_account_id, storage_key, position, is_primary, content_type,
	 file_size, width, height, file_hash)
SELECT p.account_id,
	'fixtures/profile-' || p.username || '.webp',
	1, TRUE, 'image/webp', 1, 1, 1,
	'0000000000000000000000000000000000000000000000000000000000000000'
FROM matcha_profiles p
WHERE p.username IN (
	'aliceb', 'brunom', 'camiller', 'dianep', 'eliasd',
	'fatoud', 'gabriell', 'inesm', 'julienf', 'leilag'
)
ON CONFLICT (profile_account_id, position) DO NOTHING;

INSERT INTO matcha_interests (name)
VALUES
	('photographie'), ('cuisine'), ('voyage'), ('musique'),
	('sport'), ('cinema'), ('lecture'), ('nature'), ('architecture'),
	('danse')
ON CONFLICT (LOWER(name)) DO NOTHING;

INSERT INTO matcha_profile_interests (profile_account_id, interest_id)
SELECT p.account_id, i.id
FROM (
	VALUES
		('aliceb', 'photographie'), ('aliceb', 'lecture'), ('aliceb', 'voyage'),
		('brunom', 'cuisine'), ('brunom', 'sport'), ('brunom', 'musique'),
		('camiller', 'musique'), ('camiller', 'cinema'), ('camiller', 'photographie'),
		('dianep', 'sport'), ('dianep', 'nature'), ('dianep', 'voyage'),
		('eliasd', 'architecture'), ('eliasd', 'nature'), ('eliasd', 'cinema'),
		('fatoud', 'danse'), ('fatoud', 'lecture'), ('fatoud', 'musique'),
		('gabriell', 'musique'), ('gabriell', 'sport'), ('gabriell', 'cuisine'),
		('inesm', 'nature'), ('inesm', 'voyage'), ('inesm', 'photographie'),
		('julienf', 'cuisine'), ('julienf', 'cinema'), ('julienf', 'sport'),
		('leilag', 'voyage'), ('leilag', 'cuisine'), ('leilag', 'nature')
) AS fixture(username, interest_name)
JOIN matcha_profiles p ON p.username = fixture.username
JOIN matcha_interests i ON i.name = fixture.interest_name
ON CONFLICT DO NOTHING;
