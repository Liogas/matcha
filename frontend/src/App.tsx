import { useEffect, useState } from 'react'
import './App.css'

interface DataProfilProps
{
  name: string;
  age: number;
  city: string;
  bio: string;
}

function MyProfil({name, age, city, bio}: DataProfilProps)
{

  function handleSubmit(e)
  {
    e.preventDefault();
    const form = e.target;
    const formData = new FormData(form);
    const name = formData.get("name");
    const age = Number(formData.get("age"));
    const city = formData.get("location");
    const bio = formData.get("bio");

    const requestOptions = {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({name:name, age:age, city:city, bio:bio})
    };
    fetch('http://localhost:18080/api/pushProfil/', requestOptions)
    .then(response => response.json())
    .then(data => console.log(data));
  }

  return (
    <>
      <div>
        <form onSubmit={handleSubmit}>
          <input name="name" defaultValue={name}/>
          <input name="age" defaultValue={age}/>
          <input name="location" defaultValue={city}/>
          <input name="bio" defaultValue={bio}/>
          <button type="submit">Save</button>
        </form>
      </div>
    </>
  )
}

function App() {
  const [name, setName] = useState<string>("");
  const [age, setAge] = useState<number>(0);
  const [city, setCity] = useState<string>("");
  const [bio, setBio] = useState<string>("");

  useEffect(() => {
    fetch("http://localhost:18080/api/profil")
    .then((response) => response.json())
    .then((data) => {
      setName(data["name"]);
      setCity(data["city"]);
      setAge(data["age"]);
      setBio(data["bio"]);
    })
  }, [])

  return (
    <>
      <MyProfil name={name} city={city} age={age} bio={bio}/>
    </>
  );
}

export default App
